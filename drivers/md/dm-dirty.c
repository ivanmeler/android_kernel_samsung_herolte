/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Author: Mikulas Patocka <mpatocka@redhat.com>
 *
 * Based on Chromium dm-verity driver (C) 2011 The Chromium OS Authors
 *
 * This file is released under the GPLv2.
 */

/* Author: Jitesh Shah <j1.shah@sta.samsung.com>
 * 27 March 2014.
 *
 * Updated dm-verity file to include write-able target. dm-dirty.
 * Write-able target for dm-verity.
 */


#include "dm-bufio.h"
#include <linux/module.h>
#include <linux/device-mapper.h>
#include <crypto/hash.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#define	REBUILD_TREE	1

/* Copied from dm-verity.c. Name also retained */
#define DM_VERITY_MEMPOOL_SIZE		64
#define DM_VERITY_MAX_LEVELS		63

struct dm_verity {

	struct dm_dev *data_dev;
	struct dm_dev *hash_dev;
	struct dm_target *ti;
	struct dm_bufio_client *bufio;
	char *alg_name;
	struct crypto_shash *tfm;
	u8 *root_digest;	/* digest of the root block */
	u8 *salt;		/* salt: its size is salt_size */
	unsigned salt_size;
	sector_t data_start;	/* data offset in 512-byte sectors */
	sector_t hash_start;	/* hash start in blocks */
	sector_t data_blocks;	/* the number of data blocks */
	sector_t hash_blocks;	/* the number of hash blocks */
	unsigned char data_dev_block_bits;	/* log2(data blocksize) */
	unsigned char hash_dev_block_bits;	/* log2(hash blocksize) */
	unsigned char hash_per_block_bits;	/* log2(hashes in hash block) */
	unsigned char levels;	/* the number of tree levels */
	unsigned char version;
	unsigned digest_size;	/* digest size for the current hash algorithm */
	unsigned shash_descsize;/* the size of temporary space for crypto */
	int hash_failed;	/* set to 1 if hash of any block failed */

	mempool_t *io_mempool;	/* mempool of struct dm_verity_io */
	mempool_t *vec_mempool;	/* mempool of bio vector */

	struct workqueue_struct *verify_wq;

	/* starting blocks for each tree level. 0 is the lowest level. */
	sector_t hash_level_block[DM_VERITY_MAX_LEVELS];
};

static inline u64 sector_to_block(struct dm_verity *v, u64 sector);

static sector_t verity_position_at_level(struct dm_verity *v, sector_t block,
					 int level)
{
	return block >> (level * v->hash_per_block_bits);
}

static void verity_hash_at_level(struct dm_verity *v, sector_t block, int level,
				 sector_t *hash_block, unsigned *offset)
{
	sector_t position = verity_position_at_level(v, block, level);
	unsigned idx;

	*hash_block = v->hash_level_block[level] + (position >> v->hash_per_block_bits);

	if (!offset)
		return;

	idx = position & ((1 << v->hash_per_block_bits) - 1);
	if (!v->version)
		*offset = idx * v->digest_size;
	else
		*offset = idx << (v->hash_dev_block_bits - v->hash_per_block_bits);
}

/* End of copied stuff */

struct dm_dirty_io {
	struct dm_verity *v;
	void *orig_bi_private;
	struct bio *bio;
	bio_end_io_t *orig_bi_end_io;
	struct work_struct work;
	struct bvec_iter iter;
};

#define DM_MSG_PREFIX			"dirty"

static void dirty_finish_io(struct dm_dirty_io *io, struct bio *bio, int error)
{
	bio->bi_end_io = io->orig_bi_end_io;
	bio->bi_private = io->orig_bi_private;

	bio_endio_nodec(bio, error);
}

static void dirty_hash(struct bio *bio, int error)
{
	struct bio_vec *iovec;
	int i, ret;
	u8* page;
	u8* result;
	struct shash_desc *desc;
	struct dm_dirty_io *io = bio->bi_private;
	struct dm_buffer *buf = NULL;
	u8* hash_block_buf;
	sector_t hash_block;
	unsigned offset;
	struct dm_verity *v = io->v;
	unsigned short idx = io->iter.bi_idx;

	/* At this point only WRITE bios and non-error bios are allowed.
	 * bio_data_dir() and error should have been checked before.
	 */

	/* TODO: Assumption here is that the FS sends block sized buffers. This seems to be
	 * true for ext4 but should not be assumed. Re-factor this code to remove that
	 * assumption.
	 */

	for(i = 0; i < (io->iter.bi_size >> v->data_dev_block_bits); i++) {
		/* Loop over all blocks */

		verity_hash_at_level(v, sector_to_block(v, io->iter.bi_sector) + (sector_t)i, 0, &hash_block, &offset);
		hash_block_buf = dm_bufio_read(v->bufio, hash_block, &buf);
		if (unlikely(IS_ERR(hash_block_buf))) {
			DMERR("Error getting hash block");
			buf = NULL;
			goto free_all_res;
		}

		desc = (struct shash_desc *)(io + 1);
		desc->tfm = v->tfm;
		desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
		ret = crypto_shash_init(desc);
		if (ret < 0) {
			DMERR("Error initing hash");
			goto free_all_res;
		}

		ret = crypto_shash_update(desc, v->salt, v->salt_size);
		if (ret < 0) {
			DMERR("Error updating salt hash");
			goto free_all_res;
		}

		//iovec = (bio)->bi_io_vec + i;
		iovec = bio_iovec_idx(bio, idx + i);
		/*
		if (idx + i >= bio->bi_vcnt) {
			DMERR("Excedding vcnt: idx:%hu vcnt:%hu i:%d", idx, bio->bi_vcnt, i);
			goto free_all_res;
		}
		*/

		page = kmap_atomic(iovec->bv_page);
		if(page == NULL) {
			DMERR("Failed to kmap dirty page");
			goto free_bufio;
		}
		if(iovec->bv_len != (1 << v->data_dev_block_bits)) {
			DMERR("Non-page data len: %u offset: %u", iovec->bv_len, iovec->bv_offset);
		}
		ret = crypto_shash_update(desc, page + iovec->bv_offset, iovec->bv_len);
		if (ret < 0) {
			DMERR("Error updating data hash");
		}
		kunmap_atomic(page);

		result = hash_block_buf + offset;
		ret = crypto_shash_final(desc, result);
		if (ret < 0) {
			DMERR("Error finalizing hash");
		}
        if (0 == sector_to_block(v, io->iter.bi_sector) + (sector_t)i) {
			// for data block 0, we use dummy hash
			memset(result, 1, v->digest_size);
		}
		dm_bufio_mark_buffer_dirty(buf);
free_bufio:
		dm_bufio_release(buf);
		buf = NULL;
	}


free_all_res:
	if (buf)
		dm_bufio_release(buf);
	dirty_finish_io(io, bio, error);

}

static void dirty_work(struct work_struct *w)
{
	struct dm_dirty_io *io = container_of(w, struct dm_dirty_io, work);

	dirty_hash(io->bio, 0);
}

static void dirty_end_io(struct bio *bio, int error)
{
	struct dm_dirty_io *io = bio->bi_private;

	if (error) {
		/* If there was an error writing the data, do not recalculate the hash.
		 * Just go ahead and finish the bio
		 */
		dirty_finish_io(io, bio, error);
		return;
	}

	INIT_WORK(&io->work, dirty_work);
	queue_work(io->v->verify_wq, &io->work);
}

static inline u64 sector_to_block(struct dm_verity *v, u64 sector)
{
	return (sector >> (v->data_dev_block_bits - SECTOR_SHIFT));
}

static inline unsigned bytes_to_block(struct dm_verity *v, unsigned bytes)
{
	return (bytes >> v->data_dev_block_bits);
}

static int dirty_map(struct dm_target *ti, struct bio *bio)
{
	struct dm_dirty_io *io;
	struct dm_verity *v = ti->private;

	/* If a WRITE coems with FLUSH flag set, we don't need to do anything special.
	 * Its not the end of the world if the hash is not updated to the disk right
	 * away.
	 */
	if (bio_data_dir(bio) != WRITE || bio->bi_iter.bi_size == 0)
		goto process_bio;

	if (bio->bi_iter.bi_size & ((1 << v->data_dev_block_bits) - 1) ||
		(bio->bi_iter.bi_sector & ((1 << (v->data_dev_block_bits - SECTOR_SHIFT)) - 1))) {
		DMERR("Not page size IO: size:%u, sector:%llu", bio->bi_iter.bi_size, (long long unsigned int)bio->bi_iter.bi_sector);
		return -EIO;
	}

	if(sector_to_block(v, bio->bi_iter.bi_sector) + bytes_to_block(v, bio->bi_iter.bi_size) > v->data_blocks) {
		DMERR("Out of range. sector:%llu, size:%u", (long long unsigned int)bio->bi_iter.bi_sector, bio->bi_iter.bi_size);
		return -EIO;
	}

	/* Allocate an io structure and save required info */
	io = mempool_alloc(v->io_mempool, GFP_NOIO);
	io->v = v;
	io->bio = bio;
	io->orig_bi_end_io = bio->bi_end_io;
	io->orig_bi_private = bio->bi_private;
	io->iter = bio->bi_iter;

	/* When bio is done. bi_sector points to offset from start of device, NOT the partition
	 * Size is changed to 0 (the residual size). so save these values beforehand.
	 */

	/* Link io to bio structure now and assign an end function */
	bio->bi_private = io;
	bio->bi_end_io = dirty_end_io;

process_bio:
	/* Submit bio now */
	bio->bi_bdev = v->data_dev->bdev;
	submit_bio(bio->bi_rw, bio);

	return DM_MAPIO_SUBMITTED;
}

static sector_t dirty_nr_blocks_at_level(struct dm_verity *v, int level)
{
	if (level == 0) {
		return v->hash_blocks - v->hash_level_block[0];
	} else
		return v->hash_level_block[level - 1] - v->hash_level_block[level];
}

static void dirty_one_level_up(struct dm_verity *v, sector_t block, int source_level, sector_t *hash_block, unsigned *offset)
{
	sector_t block_offset;

	block_offset = block - v->hash_level_block[source_level];
	*hash_block =  v->hash_level_block[source_level + 1] + (block_offset >> v->hash_per_block_bits);
	*offset = (block_offset & ((1 << v->hash_per_block_bits)-1)) << (v->hash_dev_block_bits - v->hash_per_block_bits);

}


static int dirty_update_hash(struct dm_verity *v, sector_t block, struct shash_desc *desc, u8 *result)
{
	struct dm_buffer *buf = NULL;
	u8* hash_block_buf;
	int ret = -1;

	hash_block_buf = dm_bufio_read(v->bufio, block, &buf);
	if (unlikely(IS_ERR(hash_block_buf))) {
		DMERR("Error getting hash block in dtr");
		buf = NULL;
		return ret;
	}

	desc->tfm = v->tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	ret = crypto_shash_init(desc);
	if (ret < 0) {
		DMERR("Error initing hash");
		goto free_all_res;
	}

	ret = crypto_shash_update(desc, v->salt, v->salt_size);
	if (ret < 0) {
		DMERR("Error updating salt hash");
		goto free_all_res;
	}

	/* Sadly assumption is hash_block size = 4096. FIX THIS */
	ret = crypto_shash_update(desc, hash_block_buf, 1 << v->hash_dev_block_bits);
	if (ret < 0) {
		DMERR("Error updating hash");
		goto free_all_res;
	}

	ret = crypto_shash_final(desc, result);
	if (ret < 0) {
		DMERR("Error finalizing hash");
		goto free_all_res;
	}

	ret = 0;
free_all_res:
	dm_bufio_release(buf);
	return ret;
}

static int finish_verity_tree(struct dm_verity *v)
{
	sector_t hash_block, nr_blocks, block_num;
	unsigned offset;
	int i, ret = -1;
	struct shash_desc *desc;
	struct dm_buffer *buf = NULL;
	u8* hash_block_buf;
	u8 *result;

	desc = kmalloc(sizeof(struct shash_desc) + crypto_shash_descsize(v->tfm) + v->digest_size, GFP_KERNEL);
	if(NULL == desc) {
		DMERR("Error allocating desc mem");
		return ret;
	}
	result = (u8*)desc + sizeof(struct shash_desc) + crypto_shash_descsize(v->tfm);

	for(i = 1; i < v->levels; i++) {
		nr_blocks = dirty_nr_blocks_at_level(v, i - 1);
		for(block_num = v->hash_level_block[i - 1]; nr_blocks > 0; nr_blocks--,block_num++) {
			dirty_one_level_up(v, block_num, i - 1, &hash_block, &offset);

			if (dirty_update_hash(v, block_num, desc, result)) {
				DMERR("Error calculating hash for block %llu", (long long unsigned int)block_num);
				goto free_desc;
			}
			hash_block_buf = dm_bufio_read(v->bufio, hash_block, &buf);
			if (unlikely(IS_ERR(hash_block_buf))) {
				DMERR("Error getting hash block %llu in dtr", (long long unsigned int)hash_block);
				buf = NULL;
				goto free_desc;
			}
			memcpy(hash_block_buf + offset, result, v->digest_size);
			dm_bufio_mark_buffer_dirty(buf);
			dm_bufio_release(buf);
		}
	}

	/* Get last level node */
	hash_block = v->hash_level_block[v->levels - 1];
	/* Calculate root hash */
	if(dirty_update_hash(v, hash_block, desc, result)) {
		DMERR("Failed to calculate root hash. Block: %llu", (long long unsigned int)hash_block);
		goto free_desc;
	}
	memcpy(v->root_digest, result, v->digest_size);
	ret = 0;

free_desc:
	kfree(desc);
	return ret;
}

static void dirty_status(struct dm_target *ti, status_type_t type,
			 unsigned status_flags, char *result, unsigned maxlen)
{
	return;
}


static int dirty_ioctl(struct dm_target *ti, unsigned cmd,
			unsigned long arg)
{
	struct dm_verity *v =  ti->private;
	struct {
		unsigned int size;
		int ret;
	} output;

	switch(cmd) {
		case REBUILD_TREE:
		if(__copy_from_user(&output.size, (void *)arg, sizeof(unsigned int))) {
			DMINFO("Error in copy_from_user");
		}

		if (output.size < v->digest_size) {
			DMERR("Allocated size (%u) less than root digest size (%u)", output.size,
					v->digest_size);
			output.size = 0;
			output.ret = -1;
			goto make_output;
		}
		/* All checks done. Lets finalize the hash */
		if (finish_verity_tree(v)) {
			output.ret = -1;
			goto make_output;
		}
		output.size = v->digest_size;
		output.ret = 0;
		if(copy_to_user((void *)(arg + sizeof(output)), v->root_digest, v->digest_size)) {
			DMERR("Failed to copy root digest");
			output.ret = -1;
		}

make_output:
		if(copy_to_user((void *)arg, &output, sizeof(output)))
			DMERR("Failed to copy to user");
		break;

		default:
		DMERR("Unknown cmd: %u", cmd);
	}

	return 0;
}


static void dirty_dtr(struct dm_target *ti)
{
	struct dm_verity *v = ti->private;
	DMINFO("dirty destructor called");

	if (v->verify_wq)
		destroy_workqueue(v->verify_wq);

	if (v->io_mempool)
		mempool_destroy(v->io_mempool);

	if (v->bufio)
		dm_bufio_write_dirty_buffers(v->bufio);
	if (v->bufio)
		dm_bufio_client_destroy(v->bufio);

	kfree(v->salt);
	kfree(v->root_digest);

	if (v->tfm)
		crypto_free_shash(v->tfm);

	kfree(v->alg_name);

	if (v->hash_dev)
		dm_put_device(ti, v->hash_dev);

	if (v->data_dev)
		dm_put_device(ti, v->data_dev);

	kfree(v);
}

static int dirty_ctr(struct dm_target *ti, unsigned argc, char **argv)
{
	struct dm_verity *v;
	unsigned num;
	unsigned long long num_ll;
	int r;
	int i;
	sector_t hash_position;
	u32 nr_hash_blocks;
	char dummy;

	v = kzalloc(sizeof(struct dm_verity), GFP_KERNEL);
	if (!v) {
		ti->error = "Cannot allocate verity structure";
		return -ENOMEM;
	}
	ti->private = v;
	v->ti = ti;

	if (argc != 10) {
		ti->error = "Invalid argument count: exactly 10 arguments required";
		r = -EINVAL;
		goto bad;
	}

	if (sscanf(argv[0], "%d%c", &num, &dummy) != 1 ||
	    num > 1) {
		ti->error = "Invalid version";
		r = -EINVAL;
		goto bad;
	}
	v->version = num;

	r = dm_get_device(ti, argv[1], FMODE_READ | FMODE_WRITE, &v->data_dev);
	if (r) {
		ti->error = "Data device lookup failed";
		goto bad;
	}

	r = dm_get_device(ti, argv[2], FMODE_READ | FMODE_WRITE, &v->hash_dev);
	if (r) {
		ti->error = "Data device lookup failed";
		goto bad;
	}

	if (sscanf(argv[3], "%u%c", &num, &dummy) != 1 ||
	    !num || (num & (num - 1)) ||
	    num < bdev_logical_block_size(v->data_dev->bdev) ||
	    num > PAGE_SIZE) {
		ti->error = "Invalid data device block size";
		r = -EINVAL;
		goto bad;
	}
	v->data_dev_block_bits = ffs(num) - 1;

	if (sscanf(argv[4], "%u%c", &num, &dummy) != 1 ||
	    !num || (num & (num - 1)) ||
	    num < bdev_logical_block_size(v->hash_dev->bdev) ||
	    num > INT_MAX) {
		ti->error = "Invalid hash device block size";
		r = -EINVAL;
		goto bad;
	}
	v->hash_dev_block_bits = ffs(num) - 1;

	if (sscanf(argv[5], "%llu%c", &num_ll, &dummy) != 1 ||
	    (sector_t)(num_ll << (v->data_dev_block_bits - SECTOR_SHIFT))
	    >> (v->data_dev_block_bits - SECTOR_SHIFT) != num_ll) {
		ti->error = "Invalid data blocks";
		r = -EINVAL;
		goto bad;
	}
	v->data_blocks = num_ll;

	if (ti->len > (v->data_blocks << (v->data_dev_block_bits - SECTOR_SHIFT))) {
		ti->error = "Data device is too small";
		r = -EINVAL;
		goto bad;
	}

	if (sscanf(argv[6], "%llu%c", &num_ll, &dummy) != 1 ||
	    (sector_t)(num_ll << (v->hash_dev_block_bits - SECTOR_SHIFT))
	    >> (v->hash_dev_block_bits - SECTOR_SHIFT) != num_ll) {
		ti->error = "Invalid hash start";
		r = -EINVAL;
		goto bad;
	}
	v->hash_start = num_ll;

	v->alg_name = kstrdup(argv[7], GFP_KERNEL);
	if (!v->alg_name) {
		ti->error = "Cannot allocate algorithm name";
		r = -ENOMEM;
		goto bad;
	}

	v->tfm = crypto_alloc_shash(v->alg_name, 0, 0);
	if (IS_ERR(v->tfm)) {
		ti->error = "Cannot initialize hash function";
		r = PTR_ERR(v->tfm);
		v->tfm = NULL;
		goto bad;
	}
	v->digest_size = crypto_shash_digestsize(v->tfm);
	if ((1 << v->hash_dev_block_bits) < v->digest_size * 2) {
		ti->error = "Digest size too big";
		r = -EINVAL;
		goto bad;
	}
	v->shash_descsize =
		sizeof(struct shash_desc) + crypto_shash_descsize(v->tfm);

	v->root_digest = kmalloc(v->digest_size, GFP_KERNEL);
	if (!v->root_digest) {
		ti->error = "Cannot allocate root digest";
		r = -ENOMEM;
		goto bad;
	}
	if (strlen(argv[8]) != v->digest_size * 2 ||
	    hex2bin(v->root_digest, argv[8], v->digest_size)) {
		ti->error = "Invalid root digest";
		r = -EINVAL;
		goto bad;
	}

	if (strcmp(argv[9], "-")) {
		v->salt_size = strlen(argv[9]) / 2;
		v->salt = kmalloc(v->salt_size, GFP_KERNEL);
		if (!v->salt) {
			ti->error = "Cannot allocate salt";
			r = -ENOMEM;
			goto bad;
		}
		if (strlen(argv[9]) != v->salt_size * 2 ||
		    hex2bin(v->salt, argv[9], v->salt_size)) {
			ti->error = "Invalid salt";
			r = -EINVAL;
			goto bad;
		}
	}

	v->hash_per_block_bits =
		fls((1 << v->hash_dev_block_bits) / v->digest_size) - 1;

	v->levels = 0;
	if (v->data_blocks)
		while (v->hash_per_block_bits * v->levels < 64 &&
		       (unsigned long long)(v->data_blocks - 1) >>
		       (v->hash_per_block_bits * v->levels))
			v->levels++;

	if (v->levels > DM_VERITY_MAX_LEVELS) {
		ti->error = "Too many tree levels";
		r = -E2BIG;
		goto bad;
	}

	hash_position = v->hash_start;
	for (i = v->levels - 1; i >= 0; i--) {
		sector_t s;
		v->hash_level_block[i] = hash_position;
		s = verity_position_at_level(v, v->data_blocks, i);
		s = (s >> v->hash_per_block_bits) +
		    !!(s & ((1 << v->hash_per_block_bits) - 1));
		if (hash_position + s < hash_position) {
			ti->error = "Hash device offset overflow";
			r = -E2BIG;
			goto bad;
		}
		hash_position += s;
	}
	v->hash_blocks = hash_position;
	/* Following typecast will only work for Small samsung disks */
	nr_hash_blocks = (u32)(hash_position - v->hash_start);

	v->bufio = dm_bufio_client_create(v->hash_dev->bdev,
		1 << v->hash_dev_block_bits, nr_hash_blocks, 0,
		NULL, NULL);
	if (IS_ERR(v->bufio)) {
		ti->error = "Cannot initialize dm-bufio";
		r = PTR_ERR(v->bufio);
		v->bufio = NULL;
		goto bad;
	}

	if (dm_bufio_get_device_size(v->bufio) < v->hash_blocks) {
		ti->error = "Hash device is too small";
		r = -E2BIG;
		goto bad;
	}

	/* We have reserved nr_hash_blocks. Prefetch all */
	dm_bufio_prefetch(v->bufio, v->hash_start, nr_hash_blocks);

	v->io_mempool = mempool_create_kmalloc_pool(DM_VERITY_MEMPOOL_SIZE,
	  sizeof(struct dm_dirty_io) + v->shash_descsize + v->digest_size);
	if (!v->io_mempool) {
		ti->error = "Cannot allocate io mempool";
		r = -ENOMEM;
		goto bad;
	}

	/* WQ_UNBOUND greatly improves performance when running on ramdisk */
	v->verify_wq = alloc_workqueue("kdirtyd", WQ_CPU_INTENSIVE | WQ_MEM_RECLAIM | WQ_UNBOUND, 1 /* num_online_cpus() */);
	if (!v->verify_wq) {
		ti->error = "Cannot allocate workqueue";
		r = -ENOMEM;
		goto bad;
	}

	DMINFO("dirty constructor succeeded\n");
	return 0;

bad:
	dirty_dtr(ti);

	return r;
}

static int dirty_iterate_devices(struct dm_target *ti,
				  iterate_devices_callout_fn fn, void *data)
{
	struct dm_verity *v = ti->private;

	return fn(ti, v->data_dev, v->data_start, ti->len, data);
}


static void dirty_io_hints(struct dm_target *ti, struct queue_limits *limits)
{
	struct dm_verity *v = ti->private;

	if (limits->logical_block_size < 1 << v->data_dev_block_bits) {
		limits->logical_block_size = 1 << v->data_dev_block_bits;
	}
	if (limits->physical_block_size < 1 << v->data_dev_block_bits) {
		limits->physical_block_size = 1 << v->data_dev_block_bits;
	}
	blk_limits_io_min(limits, limits->logical_block_size);
}

static struct target_type dirty_target = {
	.name		= "dirty",
	.version	= {1, 0, 0},
	.module		= THIS_MODULE,
	.ctr		= dirty_ctr,
	.dtr		= dirty_dtr,
	.map		= dirty_map,
	.status		= dirty_status,
	.ioctl		= dirty_ioctl,
	.iterate_devices = dirty_iterate_devices,
	.io_hints	= dirty_io_hints,
};

static int __init dm_dirty_init(void)
{
	int ret;

	ret = dm_register_target(&dirty_target);
	if (ret < 0)
		DMERR("dirty register failed %d", ret);

	return ret;
}

static void __exit dm_dirty_exit(void)
{
	dm_unregister_target(&dirty_target);
}

module_init(dm_dirty_init);
module_exit(dm_dirty_exit);

MODULE_AUTHOR("Jitesh Shah <j1.shah@sta.samsung.com>");
MODULE_DESCRIPTION(DM_NAME " update target for dm-verity");
MODULE_LICENSE("GPL");
