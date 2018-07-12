/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_LIB_FD
#define FIMC_IS_LIB_FD

#if !defined(FDAPI)
#define FDAPI
#endif

#if !defined(FDCALL)
#if defined(_STDCALL_SUPPORTED)
#define FDCALL __stdcall
#elif defined(_MSC_VER)
#if _MSC_VER >= 800
#define FDCALL __stdcall
#endif
#endif
#endif

#if !defined(FDCALL)
#define FDCALL
#endif

typedef unsigned char UINT8;
typedef short INT16;
typedef unsigned short UINT16;
typedef signed int INT32;
typedef unsigned int UINT32;
typedef int BOOL;
typedef unsigned int u32;
typedef int s32;

typedef struct {
	UINT32 structSize;
	UINT32 libVer[5];
	UINT32 apiVer;
	UINT32 features;
	const char *buildDate;
	const char *label;
} FD_INFO;

enum {
	FD_FEATURE_BLINK        = (1<<0),
	FD_FEATURE_EYE          = (1<<1),
	FD_FEATURE_SMILE        = (1<<2),
	FD_FEATURE_HW           = (1<<3),
};

FDAPI
const FD_INFO*
FDCALL
FdVersionGet(
	void
);

typedef INT32 FD_BOOL;

typedef INT32 FDSTATUS;

#define FDS_OK 0
#define FDS_NEW_VERSION ((FDSTATUS)1)
#define FDS_FAIL ((FDSTATUS)-1)
#define FDS_CANCELLED ((FDSTATUS)-2)
#define FDS_NO_SYSTEM_RESOURCES ((FDSTATUS)-3)
#define FDS_INVALID_ARG ((FDSTATUS)-4)
#define FDS_NOT_FOUND ((FDSTATUS)-5)
#define FDS_NOT_SUPPORTED ((FDSTATUS)-6)

#define IN
#define OUT
#define OPTIONAL

/* Angles ( should be in the interval [-179, 180] ) */
#define FD_ANGLE_0 0
#define FD_ANGLE_30 30
#define FD_ANGLE_45 45
#define FD_ANGLE_60 60
#define FD_ANGLE_90 90
#define FD_ANGLE_120 120
#define FD_ANGLE_135 135
#define FD_ANGLE_150 150
#define FD_ANGLE_180 180
#define FD_ANGLE_210 -150
#define FD_ANGLE_225 -135
#define FD_ANGLE_240 -120
#define FD_ANGLE_270 -90
#define FD_ANGLE_300 -60
#define FD_ANGLE_315 -45
#define FD_ANGLE_330 -30
#define FD_ANGLE_END 0x1FFF
#define FD_ANGLE_UNKNOWN 0x1FFF

enum {
	FD_FMT_UNKNOWN = 0,
	FD_FMT_GRAY8 = 1,
	FD_FMT_YUYV_422 = 2,
	FD_FMT_BGR24 = 3,
	FD_FMT_UYVY_422 = 4,
	FD_FMT_BGR32 = 5,
	FD_FMT_RGB24 = 6,
	FD_FMT_YUV_422P = 7,
	FD_FMT_YUV_422P2 = 8,
	FD_FMT_YUV_420P = 9,
	FD_FMT_YUV_444 = 10,
	FD_FMT_YUV_420P_FLAT = 11,
	FD_FMT_YUV_444_FLAT = 12,
	FD_FMT_YVYU_422 = 13,
	FD_FMT_MAX
};

enum {
	FD_MAIN_FACE_MODE_DEFAULT = 0,
	FD_MAIN_FACE_MODE_ADULT = 1,
	FD_MAIN_FACE_MODE_CHILD = 2,
	FD_MAIN_FACE_MODE_MAX
};

typedef struct FD_HEAP {
	struct FD_HEAP_VFT *vft;
} FD_HEAP;

struct FD_HEAP_VFT {

	void (FDCALL * Destroy)(
		IN FD_HEAP * heap
	);

	void* (FDCALL * Alloc)(
		IN FD_HEAP * heap,
		IN UINT32 size
	);

	void (FDCALL * Free)(
		IN FD_HEAP * heap,
		IN void *mem
	);
};

FDAPI
FDSTATUS
FDCALL
FdHeapCreate(
	IN void *buffer,
	IN UINT32 size,
	OUT FD_HEAP * *heap
);

typedef struct FD_DETECTOR {
	struct FD_DETECTOR_VFT *vft;
} FD_DETECTOR;

typedef struct FD_IMAGE_PLANE {
	INT32 stride;
	void *data;
} FD_IMAGE_PLANE;

typedef struct {
	UINT32 structSize;
	UINT32 width;
	UINT32 height;
	INT32 stride;
	UINT32  format;
	void *data;
	INT32 imageOrientation;
	void *hwData;
	FD_IMAGE_PLANE planes[3];
} FD_IMAGE;

typedef struct {
	INT32  left;
	INT32  top;
	UINT32 width;
	UINT32 height;
} FD_RECT;

typedef struct {
	FD_RECT boundRect;
	INT32   angle;
	UINT32  confidence;
	FD_BOOL isTracked;
	UINT32  nTrackedFaceID;
	UINT32 structSize;
	INT32 smileLevel;
	INT32  blinkLevel;
	INT32  blinkLevelL;
	INT32  blinkLevelR;
	FD_RECT eyeL;
	FD_RECT eyeR;
	FD_RECT mouth;
	UINT32 regionID;
	INT32 childLevel;
	INT32 yawAngle;
} FD_FACE;

enum {
	FD_REGION_TYPE_MASK = 0x000F,
	FD_REGION_TYPE_NONE = 0,
	FD_REGION_TYPE_FACE = 1,
	FD_REGION_TRACK_OUTSIDE = (1 << 8)
};

typedef struct FD_REGION {
	UINT32 structSize;
	struct FD_REGION *next;
	FD_RECT boundRect;
	UINT32 options;
	INT32 angle;
	UINT32 id;
	UINT32 eyeFactor;
} FD_REGION;

typedef struct {
	UINT32 structSize;
	UINT32 msPreviousFrame;
	UINT32 msAvailable;
	FD_IMAGE *hiResImage;
	FD_REGION *regions;
} FD_EXTRA_INFO;

enum {
	FDD_TRACKING_MODE = (1 << 0),
	FDD_COLOR_FILTER = (1 << 1),
	FDD_FAST_LOCK = (1 << 2),
	FDD_DISABLE_FACE_CONFIRMATION = (1 << 3),
	FDD_DISABLE_SQUARE_OUTPUT = (1 << 4),
	FDD_ENABLE_IMAGE_ORIENTATION = (1 << 5),
	FDD_LIMIT_FACE_SIZE = (1 << 6),
	FDD_LIMIT_FACE_ANGLE = (1 << 7),
	FDD_NEW_VERSION_CHECK = (1 << 8),
	FDD_DETECT_SMILE = (1 << 9),
	FDD_DETECT_BLINK = (1 << 10),
	FDD_DETECT_EYES = (1 << 11),
	FDD_DETECT_MOUTH = (1 << 12),
	FDD_DISABLE_AUTO_ORIENTATION = (1 << 13),
	FDD_DETECT_CHILDREN = (1 << 14),
	FDD_DISABLE_SEMIPROFILE_DETECTION = (1 << 15),
	FDD_STATIC_SECOND_SEARCH = (1 << 16),
	FDD_DISABLE_PROFILE_DETECTION = (1 << 17)
};

typedef struct {
	UINT32  structSize;
	UINT32  flags;
	UINT32  minFace;
	UINT32  maxFace;
	UINT32  centralLockAreaPercentageW;
	UINT32  centralLockAreaPercentageH;
	UINT32  centralMaxFaceNumberLimitation;
	INT16 *lockAngles;
	UINT32  framesPerLock;
	UINT32  smoothing;
	INT32  boostFDvsFP;
	UINT32  maxFaceCount;
	INT32  boostFDvsSPEED;
	UINT32  minFaceSizeFeaturesDetect;
	UINT32  maxFaceSizeFeaturesDetect;
	INT32  keepFacesOverMoreFrames;
	INT32  keepLimitingWhenNoFace;
	UINT32  framesPerLockWhenNoFaces;
} FD_DETECTOR_CFG;

enum {
	FD_PROGRESS_BEGIN = 0,
	FD_PROGRESS_UPDATE,
	FD_PROGRESS_END,
};

typedef UINT32(FDCALL * FD_PROGRESS)(
	IN void *context,
	IN UINT32 reason,
	IN UINT32 param
);

typedef struct {
	UINT32 structSize;
	const FD_IMAGE *image;
	const FD_IMAGE *imageSmall;
	const FD_RECT *faceRect;
	INT32 faceAngle;
	FD_PROGRESS progress;
	void *context;
} FD_BLINK_INPUT;

struct FD_DETECTOR_VFT {
	void (FDCALL * Destroy)(
		IN FD_DETECTOR * det
	);

	FDSTATUS(FDCALL * DetectFaces)(
		IN FD_DETECTOR * det,
		IN const FD_IMAGE *image,
		IN FD_PROGRESS progress OPTIONAL,
		IN void *context OPTIONAL
	);

	FD_FACE * (FDCALL * EnumFaces)(
		IN FD_DETECTOR * det,
		IN FD_FACE * face
	);

	UINT32 (FDCALL * GetApiVersion)(IN FD_DETECTOR * det);

	FDSTATUS(FDCALL * DetectBlink)(
		IN FD_DETECTOR * det,
		IN const FD_BLINK_INPUT * blinkInput,
		OUT INT32 *blinkLevel
	);

	FDSTATUS(FDCALL * ClearFaceList)(IN FD_DETECTOR * det);

	FDSTATUS(FDCALL * DetectFacesEx)(
		IN FD_DETECTOR * det,
		IN const FD_IMAGE *image,
		IN FD_PROGRESS progress OPTIONAL,
		IN void *context OPTIONAL,
		IN const FD_EXTRA_INFO * extraInfo OPTIONAL
	);

	FDSTATUS(FDCALL * SetMainFaceMode)(
		IN FD_DETECTOR * det,
		IN UINT32 mode
	);

	FD_FACE * (FDCALL * GetMainFace)(IN FD_DETECTOR * det);

	FDSTATUS(FDCALL * UpdateConfiguration)(
		IN FD_DETECTOR * det, IN const FD_DETECTOR_CFG * cfg);
};

FDAPI
FDSTATUS
FDCALL
FdDetectorCreate(
	IN FD_HEAP * heap OPTIONAL,
	IN const FD_DETECTOR_CFG * cfg OPTIONAL,
	OUT FD_DETECTOR * *det
);

typedef struct {
	UINT32 input_width;
	UINT32 input_height;
	UINT32 format;
	struct {
	    UINT32 stride;
	    void *data;
	} planes[3];
	UINT32 map_width;
	UINT32 map_height;
	void *map1;
	void *map2;
	void *map3;
	void *map4;
	void *map5;
	void *map6;
	void *map7;
	UINT32 up;
	UINT32 k;
	UINT32 sat;
} FDD_DATA;

typedef struct {
	u32	width;
	u32	height;
	u32	width_o;
	u32	height_o;
	u32	mapaddr;
	u32	y_addr;
	u32	cb_addr;
	u32	cr_addr;
	u32	sat;
} sFD_SW_Param;

#define MAX_DETECT_FACE	12
typedef struct {
	u32 facecnt;
	FD_FACE face[MAX_DETECT_FACE];
} sFD_FACEOUT_INFO;

void  FD_SW_Processor(sFD_SW_Param *p, sFD_FACEOUT_INFO *faceout);

#endif
