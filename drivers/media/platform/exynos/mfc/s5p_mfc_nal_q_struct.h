#ifndef __S5P_MFC_NAL_Q_STRUCT_H_
#define __S5P_MFC_NAL_Q_STRUCT_H_ __FILE__

//#define NAL_Q_ENABLE 1
//#define NAL_Q_DEBUG 1

#define NAL_Q_IN_ENTRY_SIZE		256
#define NAL_Q_OUT_ENTRY_SIZE		256

#define NAL_Q_IN_DEC_STR_SIZE		112
#define NAL_Q_IN_ENC_STR_SIZE		180
#define NAL_Q_OUT_DEC_STR_SIZE		200
#define NAL_Q_OUT_ENC_STR_SIZE		64

#define NAL_Q_IN_QUEUE_SIZE		16 // 256*16 = 4096 bytes
#define NAL_Q_OUT_QUEUE_SIZE		16 // 256*16 = 4096 bytes

typedef struct __DecoderInputStr
{
	int startcode; // = 0xAAAAAAAA; // Decoder input structure marker
	int CommandId;
	int InstanceId;
	int PictureTag;
	unsigned int CpbBufferAddr;
	int CpbBufferSize;
	int CpbBufferOffset;
	int StreamDataSize;
	int AvailableDpbFlagUpper;
	int AvailableDpbFlagLower;
	int DynamicDpbFlagUpper;
	int DynamicDpbFlagLower;
	unsigned int FirstPlaneDpb;
	unsigned int SecondPlaneDpb;
	unsigned int ThirdPlaneDpb;
	int FirstPlaneDpbSize;
	int SecondPlaneDpbSize;
	int ThirdPlaneDpbSize;
	int NalStartOptions;
	int FirstPlaneStrideSize;
	int SecondPlaneStrideSize;
	int ThirdPlaneStrideSize;
	int FirstPlane2BitDpbSize;
	int SecondPlane2BitDpbSize;
	int FirstPlane2BitStrideSize;
	int SecondPlane2BitStrideSize;
	unsigned int ScratchBufAddr;
	int ScratchBufSize;
	char reserved[NAL_Q_IN_ENTRY_SIZE - NAL_Q_IN_DEC_STR_SIZE];
} DecoderInputStr; //28*4 = 112 bytes

typedef struct __EncoderInputStr
{
	int startcode; // 0xBBBBBBBB;  // Encoder input structure marker
	int CommandId;
	int InstanceId;
	int PictureTag;
	unsigned int SourceAddr[3];
	unsigned int StreamBufferAddr;
	int StreamBufferSize;
	int StreamBufferOffset;
	int Reserved_1;
	int Reserved_2;
	int ParamChange;
	int IrSize;
	int GopConfig;
	int RcFrameRate;
	int RcBitRate;
	int MsliceMode;
	int MsliceSizeMb;
	int MsliceSizeBits;
	int FrameInsertion;
	int HierarchicalBitRateLayer[7];
	int H264RefreshPeriod;
	int HevcRefreshPeriod;
	int RcQpBound;
	int RcQpBoundPb;
	int FixedPictureQp;
	int PictureProfile;
	int BitCountEnable;
	int MaxBitCount;
	int MinBitCount;
	int NumTLayer;
	int H264NalControl;
	int HevcNalControl;
	int Vp8NalControl;
	int Vp9NalControl;
	int H264HDSvcExtension0;
	int H264HDSvcExtension1;
	int GopConfig2;
	char reserved[NAL_Q_IN_ENTRY_SIZE - NAL_Q_IN_ENC_STR_SIZE];
} EncoderInputStr; // 42*4 = 168 bytes

typedef struct __DecoderOutputStr
{
	int startcode; // 0xAAAAAAAA; // Decoder output structure marker
	int CommandId;
	int InstanceId;
	int ErrorCode;
	int PictureTagTop;
	int PictureTimeTop;
	int DisplayFrameWidth;
	int DisplayFrameHeight;
	int DisplayStatus;
	unsigned int DisplayFirstPlaneAddr;
	unsigned int DisplaySecondPlaneAddr;
	unsigned int DisplayThirdPlaneAddr;
	int DisplayFrameType;
	int DisplayCropInfo1;
	int DisplayCropInfo2;
	int DisplayPictureProfile;
	int DisplayAspectRatio;
	int DisplayExtendedAr;
	int DecodeNalSize;
	int UsedDpbFlagUpper;
	int UsedDpbFlagLower;
	int FramePackSeiAvail;
	int FramePackArrgmentId;
	int FramePackSeiInfo;
	int FramePackGridPos;
	int DisplayRecoverySeiInfo;
	int H264Info;
	int DisplayFirstCrc;
	int DisplaySecondCrc;
	int DisplayThirdCrc;
	int DisplayFirst2BitCrc;
	int DisplaySecond2BitCrc;
	int DecodedFrameWidth;
	int DecodedFrameHeight;
	int DecodedStatus;
	unsigned int DecodedFirstPlaneAddr;
	unsigned int DecodedSecondPlaneAddr;
	unsigned int DecodedThirdPlaneAddr;
	int DecodedFrameType;
	int DecodedCropInfo1;
	int DecodedCropInfo2;
	int DecodedPictureProfile;
	int DecodedRecoverySeiInfo;
	int DecodedFirstCrc;
	int DecodedSecondCrc;
	int DecodedThirdCrc;
	int DecodedFirst2BitCrc;
	int DecodedSecond2BitCrc;
	int PictureTagBot;
	int PictureTimeBot;
	char reserved[NAL_Q_OUT_ENTRY_SIZE - NAL_Q_OUT_DEC_STR_SIZE];
} DecoderOutputStr; // 50*4 =  200 bytes

typedef struct __EncoderOutputStr
{
	int startcode; // 0xBBBBBBBB; // Encoder output structure marker
	int CommandId;
	int InstanceId;
	int ErrorCode;
	int PictureTag;
	unsigned int EncodedSourceAddr[3];
	unsigned int StreamBufferAddr;
	int StreamBufferOffset;
	int StreamSize;
	int SliceType;
	int NalDoneInfo;
	unsigned int ReconLumaDpbAddr;
	unsigned int ReconChromaDpbAddr;
	int EncCnt;
	char reserved[NAL_Q_OUT_ENTRY_SIZE - NAL_Q_OUT_ENC_STR_SIZE];
} EncoderOutputStr; // 15*4 = 60 bytes

/**
 * enum nal_queue_state - The state for nal queue operation.
 */
typedef enum _nal_queue_state {
	NAL_Q_STATE_CREATED = 0,
	NAL_Q_STATE_INITIALIZED,
	NAL_Q_STATE_STARTED, // when s5p_mfc_nal_q_start() is called
	NAL_Q_STATE_STOPPED, // when s5p_mfc_nal_q_stop() is called
	NAL_Q_STATE_DESTROYED,
} nal_queue_state;

typedef struct _nal_in_queue {
	union {
		DecoderInputStr dec;
		EncoderInputStr enc;
	} entry[NAL_Q_IN_QUEUE_SIZE];
} nal_in_queue;

typedef struct _nal_out_queue {
	union {
		DecoderOutputStr dec;
		EncoderOutputStr enc;
	} entry[NAL_Q_OUT_QUEUE_SIZE];
} nal_out_queue;

struct _nal_queue_handle;
typedef struct _nal_queue_in_handle {
	struct _nal_queue_handle *nal_q_handle;
	void *in_alloc;
	unsigned int in_exe_count;
	nal_in_queue *nal_q_in_addr;
} nal_queue_in_handle;

typedef struct _nal_queue_out_handle {
	struct _nal_queue_handle *nal_q_handle;
	void *out_alloc;
	unsigned int out_exe_count;
	nal_out_queue *nal_q_out_addr;
} nal_queue_out_handle;

typedef struct _nal_queue_handle {
	nal_queue_in_handle *nal_q_in_handle;
	nal_queue_out_handle *nal_q_out_handle;
	nal_queue_state nal_q_state;
	int nal_q_ctx;
} nal_queue_handle;

#endif /* __S5P_MFC_NAL_Q_STRUCT_H_  */
