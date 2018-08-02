#ifndef __S5P_MFC_NAL_Q_H_
#define __S5P_MFC_NAL_Q_H_ __FILE__

#include "s5p_mfc_common.h"

nal_queue_handle *s5p_mfc_nal_q_create(struct s5p_mfc_dev *dev);
int s5p_mfc_nal_q_destroy(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle);

void s5p_mfc_nal_q_init(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle);
void s5p_mfc_nal_q_start(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle);
void s5p_mfc_nal_q_stop(struct s5p_mfc_dev *dev, nal_queue_handle *nal_q_handle);
void s5p_mfc_nal_q_cleanup_queue(struct s5p_mfc_dev *dev);

EncoderInputStr *s5p_mfc_nal_q_get_free_slot_in_q(struct s5p_mfc_dev *dev,
			nal_queue_in_handle *nal_q_in_handle);
int s5p_mfc_nal_q_set_in_buf(struct s5p_mfc_ctx *ctx, EncoderInputStr *pInStr);
int s5p_mfc_nal_q_get_out_buf(struct s5p_mfc_dev *dev, EncoderOutputStr *pOutStr);
int s5p_mfc_nal_q_queue_in_buf(struct s5p_mfc_dev *dev,
			nal_queue_in_handle *nal_q_in_handle);
EncoderOutputStr *s5p_mfc_nal_q_dequeue_out_buf(struct s5p_mfc_dev *dev,
			nal_queue_out_handle *nal_q_out_handle, unsigned int *reason);

int s5p_mfc_nal_q_check_single_encoder(struct s5p_mfc_ctx *ctx);
int s5p_mfc_nal_q_check_last_frame(struct s5p_mfc_ctx *ctx);
#endif /* __S5P_MFC_NAL_Q_H_  */
