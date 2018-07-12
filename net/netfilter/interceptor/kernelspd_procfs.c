/**
   @copyright
   Copyright (c) 2013 - 2015, INSIDE Secure Oy. All rights reserved.
*/

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/poll.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/cred.h>
#include <linux/proc_fs.h>
#include <linux/uidgid.h>
#endif

#include "kernelspd_internal.h"

#define KERNELSPD_PROCFS_COMMAND_BYTECOUNT_MAX 0x7fffffff
#define KERNELSPD_IPSEC_BOUNDARY_LENGTH_MAX (10*1024)

char *ipsec_boundary;
kuid_t bypass_kuid = INVALID_UID;

static struct proc_dir_entry *spd_control_file;
static int initialised = 0;
static int active = 0;
static int open = 0;
static wait_queue_head_t wait_queue;
static rwlock_t spd_proc_lock;

static struct IPSelectorFields bypass_packet_fields;
static bool bypass_packet_set;

void
spd_proc_new_bypass_packet(
        const struct IPSelectorFields *fields)
{
    write_lock(&spd_proc_lock);

    bypass_packet_fields = *fields;
    bypass_packet_set = true;

    write_unlock(&spd_proc_lock);

    wake_up_interruptible(&wait_queue);
}

static int
spd_proc_open(
        struct inode *inode,
        struct file *file)
{
    if (open != 0)
    {
        DEBUG_FAIL(proc, "Kernel SPD device already open.");
        return -EFAULT;
    }

    if (!try_module_get(THIS_MODULE))
    {
        DEBUG_FAIL(proc, "Kernel module being removed.");
        return -EFAULT;
    }

    DEBUG_HIGH(proc, "Kernel SPD device opened.");
    open = 1;

    return 0;
}

static int
update_ipsec_boundary(
        const char __user *user_boundary,
        size_t user_boundary_bytecount)
{
    char *new_ipsec_boundary = NULL;
    unsigned new_ipsec_boundary_bytecount = user_boundary_bytecount;
    int status;

    if (new_ipsec_boundary_bytecount == 0)
    {
        DEBUG_FAIL(def, "IPsec boundary needed.");
        return -EFAULT;
    }

    if (new_ipsec_boundary_bytecount > KERNELSPD_IPSEC_BOUNDARY_LENGTH_MAX)
    {
        DEBUG_FAIL(
                def,
                "IPsec boundary too long %u.",
                new_ipsec_boundary_bytecount);

        return -EFAULT;
    }

    new_ipsec_boundary = vmalloc(new_ipsec_boundary_bytecount + 1);
    if (new_ipsec_boundary == NULL)
    {
        DEBUG_FAIL(def, "vmalloc() failed.");
        return -EFAULT;
    }

    status =
        copy_from_user(
                new_ipsec_boundary,
                user_boundary,
                new_ipsec_boundary_bytecount);
    if (status != 0)
    {
        DEBUG_FAIL(def, "Copy from user failed.");
        vfree(new_ipsec_boundary);
        return -EFAULT;
    }

    new_ipsec_boundary[new_ipsec_boundary_bytecount] = '\0';
    if (ipsec_boundary_is_valid_spec(new_ipsec_boundary) == false)
    {
        DEBUG_FAIL(
                def,
                "IPsec boundary '%s' invalid.",
                new_ipsec_boundary);
        vfree(new_ipsec_boundary);
        return -EFAULT;
    }

    {
        char *old_boundary;

        write_lock_bh(&spd_lock);

        old_boundary = ipsec_boundary;
        ipsec_boundary = new_ipsec_boundary;

        write_unlock_bh(&spd_lock);

        if (old_boundary != NULL)
        {
            vfree(old_boundary);
        }
    }

    return 0;
}


int
process_command(
        struct KernelSpdCommand *cmd,
        const char __user *cmd_data,
        size_t cmd_data_bytecount)

{
    int status = 0;

    DEBUG_LOW(proc, "Processing command id %d.", cmd->command_id);

    switch (cmd->command_id)
    {
    case KERNEL_SPD_ACTIVATE:
        {
            int ipsec_boundary_bytecount = cmd_data_bytecount;

            if (active != 0)
            {
                DEBUG_FAIL(def, "Kernel SPD already active.");
                status = -EFAULT;
            }
            else
            {
                status =
                    update_ipsec_boundary(
                            cmd_data,
                            ipsec_boundary_bytecount);

                if (status != 0)
                {
                    break;
                }

                if (spd_hooks_init() != 0)
                {
                    DEBUG_FAIL(proc, "Kernel SPD Failed activating NF Hooks.");
                    status = -EFAULT;
                }

                active = 1;

                DEBUG_HIGH(
                        kernel,
                        "Kernel SPD activated. IPsec boundary: '%s'.",
                        ipsec_boundary);
            }
        }
        break;

    case KERNEL_SPD_DEACTIVATE:
        {
            if (active == 0)
            {
                DEBUG_FAIL(proc, "Kernel SPD not active.");
            }
            else
            {
                DEBUG_HIGH(proc, "Kernel SPD deactivated.");
                spd_hooks_uninit();
                bypass_kuid = INVALID_UID;
            }

            active = 0;
        }
        break;

    case KERNEL_SPD_INSERT_ENTRY:
        {
            struct IPSelectorDbEntry *entry;
            const int payload_bytecount = cmd_data_bytecount;

            if (!KERNEL_SPD_ID_VALID(cmd->spd_id))
            {
                DEBUG_FAIL(
                        proc,
                        "Invalid SPD id %d.",
                        cmd->spd_id);

                status = -EFAULT;
                break;
            }

            entry = vmalloc(sizeof *entry + payload_bytecount);
            if (entry == NULL)
            {
                DEBUG_FAIL(
                        proc,
                        "vmalloc(%d) failed.",
                        (int) (sizeof *entry + payload_bytecount));

                status = -EFAULT;
                break;
            }

            status = copy_from_user(entry + 1, cmd_data, payload_bytecount);
            if (status != 0)
            {
                DEBUG_FAIL(proc, "Copy from user failed.");

                vfree(entry);
                status = -EFAULT;
                break;
            }

            entry->action = cmd->action_id;
            entry->id = cmd->entry_id;
            entry->priority = cmd->priority;

            if (ip_selector_db_entry_check(
                        entry,
                        sizeof *entry + payload_bytecount)
                < 0)
            {
                DEBUG_FAIL(proc, "Selector check failed.");

                vfree(entry);
                status = -EFAULT;
                break;
            }

            DEBUG_DUMP(
                    proc,
                    debug_dump_ip_selector_group,
                    entry + 1,
                    payload_bytecount,
                    "Insert entry %d to spd "
                    "id %d action %d priority %d precedence %d:",
                    entry->id,
                    cmd->spd_id,
                    entry->action,
                    entry->priority,
                    cmd->precedence);

            write_lock_bh(&spd_lock);
            ip_selector_db_entry_add(
                    &spd,
                    cmd->spd_id,
                    entry,
                    cmd->precedence);
            write_unlock_bh(&spd_lock);
        }
        break;

    case KERNEL_SPD_REMOVE_ENTRY:

        if (!KERNEL_SPD_ID_VALID(cmd->spd_id))
        {
            DEBUG_FAIL(
                    proc,
                    "Invalid SPD id %d.",
                    cmd->spd_id);

            status = -EFAULT;
            break;
        }

        {
            struct IPSelectorDbEntry *removed;

            write_lock_bh(&spd_lock);
            removed =
                ip_selector_db_entry_remove(
                        &spd,
                        cmd->spd_id,
                        cmd->entry_id);

            write_unlock_bh(&spd_lock);

            if (removed != NULL)
            {
                DEBUG_DUMP(
                        proc,
                        debug_dump_ip_selector_group,
                        removed + 1,
                        -1,
                        "Removed entry %d to spd id %d action %d "
                        "priority %d:",
                        removed->id,
                        cmd->spd_id,
                        removed->action,
                        removed->priority);

                vfree(removed);
            }
            else
            {
                DEBUG_FAIL(
                        proc,
                        "Remove failed: Entry %d not found from spd id %d.",
                        cmd->entry_id,
                        cmd->spd_id);
            }
        }

        break;

    case KERNEL_SPD_UPDATE_IPSEC_BOUNDARY:
        {
            int new_ipsec_boundary_bytecount = cmd_data_bytecount;

            if (active == 0)
            {
                DEBUG_FAIL(def, "Kernel SPD is not active.");
                return -EFAULT;
            }

            status =
                update_ipsec_boundary(
                        cmd_data,
                        new_ipsec_boundary_bytecount);

            if (status != 0)
            {
                break;
            }

            DEBUG_HIGH(
                    kernel,
                    "IPsec boundary updated: '%s'.",
                    ipsec_boundary);
        }
        break;

    case KERNEL_SPD_VERSION_SYNC:
        {
            int version_bytecount = cmd_data_bytecount;
            uint32_t version;

            if (version_bytecount != sizeof version)
            {
                DEBUG_FAIL(
                        def,
                        "Invalid version size %d; should be %d.",
                        version_bytecount,
                        (int) sizeof version);
                return -EFAULT;
            }

            status =
                copy_from_user(
                        &version,
                        cmd_data,
                        sizeof version);
            if (status != 0)
            {
                DEBUG_FAIL(def, "Copy from user failed.");
                return -EFAULT;
            }

            if (version != KERNEL_SPD_VERSION)
            {
                DEBUG_FAIL(
                        def,
                        "Invalid version %d; should be %d.",
                        version,
                        KERNEL_SPD_VERSION);

                return -EINVAL;
            }

            DEBUG_HIGH(
                    kernel,
                    "Versions in sync: %d.",
                    version);
        }
        break;

    case KERNEL_SPD_ADD_BYPASS_UID:
        {
            uint32_t uid;

            if (active == 0)
            {
                DEBUG_FAIL(def, "Kernel SPD is not active.");
                return -EFAULT;
            }

            status =
                copy_from_user(
                        &uid,
                        cmd_data,
                        sizeof uid);
            if (status != 0)
            {
                DEBUG_FAIL(def, "Copy from user failed.");
                return -EFAULT;
            }

            write_lock_bh(&spd_lock);
            bypass_kuid = make_kuid(current_user_ns(), (uid_t) uid);
            write_unlock_bh(&spd_lock);

            DEBUG_HIGH(
                    kernel,
                    "Set bypass uid to %u.",
                    uid);
        }
        break;



    default:
        DEBUG_FAIL(proc, "Unknown command id %d.", cmd->command_id);
        break;
    }

    DEBUG_LOW(proc, "Returning %d", status);

    return status;
}


static ssize_t
spd_proc_write(
        struct file *file,
        const char __user *data,
        size_t data_len,
        loff_t *pos)
{
    size_t bytes_read = 0;

    DEBUG_LOW(proc, "Write of %d bytes.", (int) data_len);
    while (bytes_read < data_len)
    {
        struct KernelSpdCommand command;
        int status;

        if (data_len < sizeof command)
        {
            DEBUG_FAIL(
                    proc,
                    "Data length %d less than sizeof command %d bytes.",
                    (int) data_len,
                    (int) sizeof command);

            bytes_read = -EFAULT;
            break;
        }

        status = copy_from_user(&command, data, sizeof command);
        if (status != 0)
        {
            DEBUG_FAIL(proc, "Copy from user failed.");
            bytes_read = -EFAULT;
            break;
        }

        if (command.bytecount < sizeof command)
        {
            DEBUG_FAIL(
                    proc,
                    "Command bytecount %d less than command size %d.",
                    (int) command.bytecount,
                    (int) sizeof command);

            bytes_read = -EINVAL;
            break;
        }

        if (command.bytecount > KERNELSPD_PROCFS_COMMAND_BYTECOUNT_MAX)
        {
            DEBUG_FAIL(
                    proc,
                    "Command bytecount %d bigger than max command size %d.",
                    (int) command.bytecount,
                    (int) KERNELSPD_PROCFS_COMMAND_BYTECOUNT_MAX);

            bytes_read = -EINVAL;
            break;
        }

        if (command.bytecount > data_len - bytes_read)
        {
            DEBUG_FAIL(
                    proc,
                    "Command bytecount %d bigger than data_len %d.",
                    (int) command.bytecount,
                    (int) data_len);

            bytes_read = -EINVAL;
            break;
        }

        bytes_read += sizeof command;

        status =
            process_command(
                    &command,
                    data + bytes_read,
                    command.bytecount - sizeof command);

        if (status < 0)
        {
            bytes_read = status;
            break;
        }

        bytes_read += command.bytecount;
    }

    return bytes_read;
}


static void
spd_proc_cleanup_selectors(
        void)
{
    for (;;)
    {
        struct IPSelectorDbEntry *entry;

        write_lock_bh(&spd_lock);
        entry = ip_selector_db_entry_remove_next(&spd);
        write_unlock_bh(&spd_lock);

        if (entry != NULL)
        {
            vfree(entry);
        }
        else
        {
            break;
        }
    }
}


static int
spd_proc_release(
        struct inode *inode,
        struct file *file)
{
    DEBUG_HIGH(proc, "Kernel SPD device closed.");

    if (open != 0)
    {
        DEBUG_HIGH(proc, "Kernel SPD deactivated.");
        spd_hooks_uninit();
        active = 0;

        spd_proc_cleanup_selectors();
        module_put(THIS_MODULE);
        open = 0;
    }

    return 0;
}


static ssize_t
spd_proc_read(
        struct file *file,
        char __user *buf,
        size_t len,
        loff_t *pos)
{
    size_t read_len = 0;

    write_lock_bh(&spd_proc_lock);

    if (bypass_packet_set == true)
    {
        int status = 0;

        struct KernelSpdCommand command = { 0 };

        command.command_id = KERNEL_SPD_EVENT_BYPASS_FIELDS;
        command.bytecount = sizeof command + sizeof bypass_packet_fields;

        if (len < command.bytecount)
        {
            status = -EFAULT;
        }

        if (status == 0)
        {
            status = copy_to_user(buf, &command, sizeof command);
        }

        if (status == 0)
        {
            status =
                copy_to_user(
                        buf + sizeof command,
                        &bypass_packet_fields,
                        sizeof bypass_packet_fields);
        }

        if (status != 0)
        {
            read_len = status;
        }
        else
        {
            read_len = command.bytecount;
            *pos = read_len;
        }

        bypass_packet_set = false;
    }
    else if (file->f_flags & O_NONBLOCK)
    {
        read_len = -EAGAIN;
    }

    write_unlock_bh(&spd_proc_lock);

    return read_len;
}

static unsigned int
spd_proc_poll(
        struct file *file,
        struct poll_table_struct *table)
{
    unsigned int mask = 0;

    poll_wait(file, &wait_queue, table);

    write_lock_bh(&spd_proc_lock);

    if (bypass_packet_set == true)
    {
        mask |= (POLLIN | POLLRDNORM);
    }

    write_unlock_bh(&spd_proc_lock);

    return mask;
}

static const struct file_operations spd_proc_fops =
{
    .open = spd_proc_open,
    .write = spd_proc_write,
    .release = spd_proc_release,
    .read = spd_proc_read,
    .poll = spd_proc_poll
};

void spd_proc_set_ids(struct proc_dir_entry *proc_entry)
{
    uid_t uid = 0;
    gid_t gid = 0;

#ifdef CONFIG_VPNCLIENT_PROC_UID
    uid = CONFIG_VPNCLIENT_PROC_UID;
#endif

#ifdef CONFIG_VPNCLIENT_PROC_GID
    gid = CONFIG_VPNCLIENT_PROC_GID;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    proc_entry->uid = uid;
    proc_entry->gid = gid;
#else
    proc_set_user(proc_entry,
                  make_kuid(current_user_ns(), uid),
                  make_kgid(current_user_ns(), gid));
#endif
}

int
spd_proc_init(
        void)
{
    spd_control_file =
        proc_create(
                LINUX_SPD_PROC_FILENAME,
                S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
                NULL, &spd_proc_fops);

    if (spd_control_file == NULL)
    {
        DEBUG_FAIL(
                proc,
                "Failure creating proc entry %s.",
                LINUX_SPD_PROC_FILENAME);

        return -1;
    }

    spd_proc_set_ids(spd_control_file);

    init_waitqueue_head(&wait_queue);
    rwlock_init(&spd_proc_lock);

    initialised = 1;

    DEBUG_MEDIUM(
            proc,
            "Created proc entry %s.",
            LINUX_SPD_PROC_FILENAME);

    return 0;
}


void
spd_proc_uninit(
        void)
{
    if (initialised != 0)
    {
        DEBUG_MEDIUM(
                proc,
                "Removing proc entry %s.",
                LINUX_SPD_PROC_FILENAME);

        DEBUG_HIGH(proc, "Kernel SPD deactivated.");
        spd_hooks_uninit();
        active = 0;

        spd_proc_cleanup_selectors();

        remove_proc_entry(LINUX_SPD_PROC_FILENAME, NULL);

        if (ipsec_boundary != NULL)
        {
            vfree(ipsec_boundary);
            ipsec_boundary = NULL;
        }

        bypass_kuid = INVALID_UID;
        initialised = 0;
    }
    else
    {
        DEBUG_FAIL(proc, "Already uninitialised.");
    }
}
