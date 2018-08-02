/*
 *
 *
 */
#include "cy8cmbr_swd.h"

/*****************************************************************************
 *
 *****************************************************************************/
struct swd_data {
	struct touchkey_i2c *swd_tkey_i2c;
	struct device *dev;
	int (*power) (void *, bool on);
	int swdio_gpio;
	int swdck_gpio;
	const u8 *fw_img;
	size_t fw_img_size;
};
static struct swd_data _swd_data;
static struct swd_data *g_sd = NULL;

/*****************************************************************************
 * SWD
 *****************************************************************************/
/* Macros for SWD Physical layer functions */
#define swd_set_swdck_out(sd)	gpio_direction_output(sd->swdck_gpio, 0)
#define swd_set_swdck_hi(sd)	gpio_set_value(sd->swdck_gpio, 1)
#define swd_set_swdck_lo(sd)	gpio_set_value(sd->swdck_gpio, 0)

#define swd_set_swdio_out(sd)	gpio_direction_output(sd->swdio_gpio, 0)
#define swd_set_swdio_in(sd)	gpio_direction_input(sd->swdio_gpio)
#define swd_set_swdio_hi(sd)	gpio_set_value(sd->swdio_gpio, 1)
#define swd_set_swdio_lo(sd)	gpio_set_value(sd->swdio_gpio, 0)
#define swd_get_swdio(sd)	gpio_get_value(sd->swdio_gpio)

/*****************************************************************************
 * AN84858
 *
 *
 *
 *
 *****************************************************************************/
#include "SWD_UpperPacketLayer.h"
#include "SWD_PacketLayer.h"

//#define HEX_FIRMWARE
#define FORCE_PROTECT

#define CY8C40xx_FAMILY				1

/******************************************************************************
 *   Timeout
 ******************************************************************************/
#define DEVICE_ACQUIRE_TIMEOUT_US		50000
#define SROM_POLLING_TIMEOUT_MS			1000

/******************************************************************************
 *   Hex
 ******************************************************************************/
/* The below definitions are not dependent on hex file and are always
   constant */

#if (CY8C40xx_FAMILY)
#define BYTES_PER_FLASH_ROW			64
#else
#define BYTES_PER_FLASH_ROW			128
#endif
#define SILICON_ID_BYTE_LENGTH			4
#define CHECKSUM_BYTE_LENGTH			2
#define MAXIMUM_ROW_PROTECTION_BYTE_LENGTH	32
#ifndef HEX_FIRMWARE
#define NUMBER_OF_FLASH_ROWS_HEX_FILE		128
#endif
#ifdef HEX_FIRMWARE
#include "HexImage.h"
static void HEX_ReadSiliconId(u32 * hexSiliconId)
{
	u8 i;

	for (i = 0; i < SILICON_ID_BYTE_LENGTH; i++) {
		*hexSiliconId =
			*hexSiliconId | ((u32) (deviceSiliconId_HexFile[i]) <<
					(8 * i));
	}
}

static void HEX_ReadRowData(u16 rowCount, u8 * rowData)
{
	u16 i;			/* Maximum value of 'i' can be 256 */

	for (i = 0; i < BYTES_PER_FLASH_ROW; i++) {
		rowData[i] = flashData_HexFile[rowCount][i];
	}
}

static void HEX_ReadChipProtectionData(u8 * chipProtectionData)
{
	*chipProtectionData = chipProtectionData_HexFile;
}

static void HEX_ReadRowProtectionData(u8 rowProtectionByteSize,
		u8 * rowProtectionData)
{
	u16 i;

	for (i = 0; i < rowProtectionByteSize; i++) {
		rowProtectionData[i] = flashProtectionData_HexFile[i];
	}
}

static void HEX_ReadChecksumData(u16 * checksumData)
{
	u8 i;

	for (i = 0; i < CHECKSUM_BYTE_LENGTH; i++) {
		*checksumData |= (checksumData_HexFile[i] << (8 * i));
	}
}
#endif //HEX_FIRMWARE
static u16 GetFlashRowCount(void)
{
	return (NUMBER_OF_FLASH_ROWS_HEX_FILE);
}

/******************************************************************************
 *   Programming Steps
 ******************************************************************************/
/* Return value definitions for high level Programming functions */
#define FAILURE 0
#define SUCCESS (!FAILURE)

/* Return value of PollSromStatus function */
#define SROM_SUCCESS 0x01

/* Error codes returned by Swd_PacketAck whenever any top-level step returns a
   failure status */

/* This bit field is set if programmer fails to acquire the device in 1.5 ms */
#define PORT_ACQUIRE_TIMEOUT_ERROR	0x10

/* This bit field is set if the SROM does not return the success status code
   within the SROM Polling timeout duration*/
#define SROM_TIMEOUT_ERROR		0x20

/* This bit field is set in case of JTAG ID mismatch or Flash data verification
   mismatch or Checksum data mismatch */
#define VERIFICATION_ERROR			0x40

/* This bit field is set if wrong transition of chip protection settings is
   detected */
#define TRANSITION_ERROR			0x80

/* Constants for Address Space of CPU */
#if (CY8C40xx_FAMILY)
#define CPUSS_SYSREQ	            0x40100004
#define CPUSS_SYSARG	            0x40100008
#else
#define CPUSS_SYSREQ	            0x40000004
#define CPUSS_SYSARG	            0x40000008
#endif
#define TEST_MODE	                0x40030014
#define SRAM_PARAMS_BASE	        0x20000100
#define SFLASH_MACRO	            0x0FFFF000
#define SFLASH_CPUSS_PROTECTION	    0x0FFFF07C

/* SROM Constants */
#define SROM_KEY1	                0xB6
#define SROM_KEY2	                0xD3
#define SROM_SYSREQ_BIT	            0x80000000
#define SROM_PRIVILEGED_BIT	        0x10000000
#define SROM_STATUS_SUCCEEDED	    0xA0000000
#define SROM_STATUS_FAILED	        0xF0000000

/* SROM Commands */
#define SROM_CMD_GET_SILICON_ID	    0x00
#define SROM_CMD_LOAD_LATCH	        0x04
#define SROM_CMD_PROGRAM_ROW	    0x06
#define SROM_CMD_ERASE_ALL	        0x0A
#define SROM_CMD_CHECKSUM	        0x0B
#define SROM_CMD_WRITE_PROTECTION	0x0D
#if (CY8C40xx_FAMILY)
#define SROM_CMD_SET_IMO_48MHZ		0x15
#endif

/* Chip Level Protection */
#define CHIP_PROT_VIRGIN	        0x00
#define CHIP_PROT_OPEN	            0x01
#define CHIP_PROT_PROTECTED	        0x02
#define CHIP_PROT_KILL	            0x04

/* Mask for status code returned by polling SROM */
#define SROM_STATUS_SUCCESS_MASK	0xF0000000

/* Constant for computing checksum of entire flash */
#define CHECKSUM_ENTIRE_FLASH		0x8000

/* Constant DAP ID of ARM Cortex M0 CPU */
#define CM0_DAP_ID					0x0BB11477

/* Constant maximum number of rows in PSoC 4 */
#define ROWS_PER_ARRAY				256

static u32 checksum_Privileged = 0;
static u32 statusCode = 0;

static u8 result = 0;
#if defined(HEX_FIRMWARE) || defined(FORCE_PROTECT)
static u8 chipProtectionData_Chip = 0;
#endif
static u8 randompara = 0;

static enum Transition_mode { OPEN_XXX, VIRGIN_OPEN, PROT_XXX,
	WRONG_TRANSITION } flow;

#if defined(FORCE_PROTECT)
static void HEX_ReadChipProtectionData(u8 * chipProtectionData)
{
	*chipProtectionData = CHIP_PROT_OPEN;
}

static void HEX_ReadRowProtectionData(u8 rowProtectionByteSize, u8 * rowProtectionData)
{
	u16 i;

	for(i = 0; i < rowProtectionByteSize; i++)
		rowProtectionData[i] = 0xFF;
}
#endif

static u8 PollSromStatus(void)
{
	unsigned long end_time;

	end_time = jiffies + msecs_to_jiffies(SROM_POLLING_TIMEOUT_MS);
	do {
		/* Read CPUSS_SYSREQ register and check if SROM_SYSREQ_BIT and
		   SROM_PRIVILEGED_BIT are reset to 0 */
		Read_IO(CPUSS_SYSREQ, &statusCode);

		statusCode &= (SROM_SYSREQ_BIT | SROM_PRIVILEGED_BIT);
	} while ((statusCode != 0) && time_before_eq(jiffies, end_time));

	randompara = 10;
	/* If time exceeds the timeout value, set the SROM_TIMEOUT_ERROR bit in
	   swd_PacketAck */
	if (!time_before_eq(jiffies, end_time)) {
		swd_PacketAck = swd_PacketAck | SROM_TIMEOUT_ERROR;

		Read_IO(CPUSS_SYSARG, &statusCode);

		return (FAILURE);
	}
	randompara = 12;
	/* Read CPUSS_SYSARG register to check if the SROM command executed
	   successfully else set SROM_TIMEOUT_ERROR in swd_PacketAck */
	Read_IO(CPUSS_SYSARG, &statusCode);

	if ((statusCode & SROM_STATUS_SUCCESS_MASK) != SROM_STATUS_SUCCEEDED) {
		swd_PacketAck = swd_PacketAck | SROM_TIMEOUT_ERROR;
		randompara = 14;
		return (FAILURE);
	} else {
		return (SUCCESS);
	}
}

#if (CY8C40xx_FAMILY)
static void SetIMO48MHz(void)
{
	u32 parameter1 = 0;

	/* Load the Parameter1 with the SROM command to read silicon ID */
	parameter1 = (u32) (((u32) SROM_KEY1 << 0) +	//
			(((u32) SROM_KEY2 +
			  (u32) SROM_CMD_SET_IMO_48MHZ) << 8));

	/* Write the command to CPUSS_SYSARG register */
	Write_IO(CPUSS_SYSARG, parameter1);
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_SET_IMO_48MHZ);
}

#endif

#ifdef HEX_FIRMWARE
static u8 GetChipProtectionVal(void)
{
	u32 parameter1 = 0;
	u32 chipProtData = 0;

	/* Load the Parameter1 with the SROM command to read silicon ID */
	parameter1 = (u32) (((u32) SROM_KEY1 << 0) +	//
			(((u32) SROM_KEY2 +
			  (u32) SROM_CMD_GET_SILICON_ID) << 8));

	/* Write the command to CPUSS_SYSARG register */
	Write_IO(CPUSS_SYSARG, parameter1);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Request system call by writing to CPUSS_SYSREQ register */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_GET_SILICON_ID);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		return (FAILURE);
	}

	/* Read CPUSS_SYSREQ register to get the current protection setting of the
	   chip */
	Read_IO(CPUSS_SYSREQ, &chipProtData);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	chipProtectionData_Chip = (u8) (chipProtData >> 12);
	input_info(true, g_sd->dev, "%s: chipProtectionData_Chip=%d\n", __func__,
			chipProtectionData_Chip);
	return (SUCCESS);
}

static u8 GetTransitionMode(void)
{
	u8 chipProtectionData_Hex;

	/* Get the chip protection setting in the HEX file */
	HEX_ReadChipProtectionData(&chipProtectionData_Hex);

	/* enum variable flow stores the transition (current protection setting to
	   setting in hex file) of the chip */
	flow = WRONG_TRANSITION;

	input_info(true, g_sd->dev, "%s: chipProtectionData_Chip=%s\n", __func__,
			chipProtectionData_Chip ==
			CHIP_PROT_VIRGIN ? "CHIP_PROT_VIRGIN" : chipProtectionData_Chip
			==
			CHIP_PROT_OPEN ? "CHIP_PROT_OPEN" : chipProtectionData_Chip ==
			CHIP_PROT_PROTECTED ? "CHIP_PROT_PROTECTED" : "ELSE");
	switch (chipProtectionData_Chip) {
		/* virgin to open protection setting is the only allowed transition */
	case CHIP_PROT_VIRGIN:
		if (chipProtectionData_Hex == CHIP_PROT_OPEN)
			flow = VIRGIN_OPEN;
		else
			flow = WRONG_TRANSITION;
		break;

		/* All transitions from Open are allowed other than transition to virgin
		   mode */
	case CHIP_PROT_OPEN:
		if (chipProtectionData_Hex == CHIP_PROT_VIRGIN)
			flow = WRONG_TRANSITION;
		else
			flow = OPEN_XXX;
		break;

		/* Protected to Protected and Protected to Open are the allowed
		   transitions */
	case CHIP_PROT_PROTECTED:
		if ((chipProtectionData_Hex == CHIP_PROT_OPEN)
				|| (chipProtectionData_Hex == CHIP_PROT_PROTECTED))
			flow = PROT_XXX;
		else
			flow = WRONG_TRANSITION;
		break;

	default:
		flow = WRONG_TRANSITION;
		break;
	}

	/* Set TRANSITION_ERROR bit high in Swd_PacketAck to show wrong transition
	   error */
	if (flow == WRONG_TRANSITION) {
		swd_PacketAck = swd_PacketAck | TRANSITION_ERROR;
		return (FAILURE);
	}
	input_info(true, g_sd->dev, "%s: flow=%s\n", __func__,
			flow == OPEN_XXX ? "OPEN_XXX" :
			flow == VIRGIN_OPEN ? "VIRGIN_OPEN" :
			flow == PROT_XXX ? "PROT_XXX" :
			flow == WRONG_TRANSITION ? "WRONG_TRANSITION" : "");
	return (SUCCESS);
}
#endif //HEX_FIRMWARE

static u8 LoadLatch(u8 arrayID, u8 * rowData)
{
	u32 parameter1 = 0;
	u32 parameter2 = 0;
	u8 i = 0;

	/* Load parameter1 with the SROM command to load the page latch buffer
	   with programming data */
	parameter1 = ((u32) SROM_KEY1 << 0) +	//
		(((u32) SROM_KEY2 + (u32) SROM_CMD_LOAD_LATCH) << 8) +	//
		(0x00 << 16) + ((u32) arrayID << 24);

	/* Number of Bytes to load minus 1 */
	parameter2 = (BYTES_PER_FLASH_ROW - 1);

	/* Write parameter1 in SRAM */
	Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Write parameter2 in SRAM */
	Write_IO(SRAM_PARAMS_BASE + 0x04, parameter2);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Put row data into SRAM buffer */
	for (i = 0; i < BYTES_PER_FLASH_ROW; i += 4) {
		parameter1 =
			(rowData[i] << 0) + (rowData[i + 1] << 8) +
			(rowData[i + 2] << 16) + (rowData[i + 3] << 24);

		/* Write parameter1 in SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x08 + i, parameter1);

		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

	}

	/*  Call "Load Latch" SROM API */

	/* Set location of parameters */
	Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_LOAD_LATCH);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		return (FAILURE);
	}
	return (SUCCESS);
}

#if defined(HEX_FIRMWARE) || defined(FORCE_PROTECT)
static u8 LoadLatch_protect(u8 arrayID, u8 * rowData, u8 rowProtectionByteSize)
{
	u32 parameter1 = 0;
	u32 parameter2 = 0;
	u8 i = 0;

	/* Load parameter1 with the SROM command to load the page latch buffer
	   with programming data */
	parameter1 = ((u32) SROM_KEY1 << 0) +	//
		(((u32) SROM_KEY2 + (u32) SROM_CMD_LOAD_LATCH) << 8) +	//
		(0x00 << 16) + ((u32) arrayID << 24);

	/* Number of Bytes to load minus 1 */
	parameter2 = (rowProtectionByteSize - 1);

	/* Write parameter1 in SRAM */
	Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Write parameter2 in SRAM */
	Write_IO(SRAM_PARAMS_BASE + 0x04, parameter2);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Put row data into SRAM buffer */
	for (i = 0; i < rowProtectionByteSize; i += 4) {
		parameter1 =
			(rowData[i] << 0) + (rowData[i + 1] << 8) +
			(rowData[i + 2] << 16) + (rowData[i + 3] << 24);

		/* Write parameter1 in SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x08 + i, parameter1);

		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

	}

	/*  Call "Load Latch" SROM API */

	/* Set location of parameters */
	Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_LOAD_LATCH);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		return (FAILURE);
	}
	return (SUCCESS);
}
#endif //HEX_FIRMWARE

static u8 ChecksumAPI(u16 checksumRow, u32 * checksum)
{
	u32 parameter1 = 0;
	u32 checksum_chip = 0;

	/* Load parameter1 with the SROM command to compute checksum of whole
	   flash */
	parameter1 = ((u32) SROM_KEY1 << 00) +
			(((u32) SROM_KEY2 + (u32) SROM_CMD_CHECKSUM) << 8) +
			(((u32) checksumRow & 0x000000FF) << 16) +
			(((u32) checksumRow & 0x0000FF00) << 16);

	/* Load CPUSS_SYSARG register with parameter1 command */
	Write_IO(CPUSS_SYSARG, parameter1);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_CHECKSUM);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		return (FAILURE);
	}

	/* Read CPUSS_SYSARG register to get the checksum value */
	Read_IO(CPUSS_SYSARG, &checksum_chip);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* 28-bit checksum */
	*checksum = (checksum_chip & 0x0FFFFFFF);

	return (SUCCESS);
}

#define SetXresHigh()		g_sd->power(g_sd->swd_tkey_i2c, 1)
#define SetXresLow()		g_sd->power(g_sd->swd_tkey_i2c, 0)
static u8 DeviceAcquire(void)
{
	u32 chip_DAP_Id = 0;
	u16 total_packet_count = 0;
	u32 status = 0;
	unsigned long end_time;
	struct swd_data *sd = g_sd;

	input_dbg(true, g_sd->dev, "%s: \n", __func__);
	/* Aquiring Sequence */

	/* Set SWDIO and SWDCK initial pin directions and out values */
	swd_set_swdio_out(sd);
	swd_set_swdio_lo(sd);
	swd_set_swdck_out(sd);
	swd_set_swdck_lo(sd);

	/* Set XRES of PSoC 4 low for 100us with SWDCK and SWDIO low (min delay
	   required is 5us) */
	SetXresLow();
	msleep(100);	//DelayHundredUs();
	SetXresHigh();

	end_time = jiffies + usecs_to_jiffies(DEVICE_ACQUIRE_TIMEOUT_US);
	do {
		/* Call Swd_LineReset (Standard ARM command to reset DAP) and read
		   DAP_ID from chip */
		Swd_LineReset();

		Read_DAP(DPACC_DP_IDCODE_READ, &chip_DAP_Id);

		total_packet_count++;

	} while ((swd_PacketAck != SWD_OK_ACK)
			&& time_before_eq(jiffies, end_time));

	/* Set PORT_ACQUIRE_TIMEOUT_ERROR bit in swd_PacketAck if time
	   exceeds 1.5 ms */
	if (!time_before_eq(jiffies, end_time)) {
		swd_PacketAck = swd_PacketAck | PORT_ACQUIRE_TIMEOUT_ERROR;
		input_err(true, g_sd->dev, "%s: fail, swd_PacketAck=0x%02x \n",
				__func__, swd_PacketAck);
		goto testmode_fail;	//return(FAILURE);
	}
	input_info(true, g_sd->dev, "%s: chip_DAP_Id=0x%08x \n", __func__, chip_DAP_Id);

	/* Set VERIFICATION_ERROR bit in swd_PacketAck if the DAP_ID read
	   from chip does not match with the ARM CM0_DAP_ID (MACRO defined in
	   ProgrammingSteps.h file - 0x0BB11477) */
	if (chip_DAP_Id != CM0_DAP_ID) {
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		input_dbg(true, g_sd->dev, "%s:%d: chip_DAP_Id:%x\n", __func__, __LINE__, chip_DAP_Id);
		goto testmode_fail;	//return(FAILURE);
	}

	/* Initialize Debug Port */
	Write_DAP(DPACC_DP_CTRLSTAT_WRITE, 0x54000000);

	if (swd_PacketAck != SWD_OK_ACK) {
		input_dbg(true, g_sd->dev, "%s:%d: swd_PacketAck:%x\n", __func__, __LINE__, swd_PacketAck);
		goto testmode_fail;	//return(FAILURE);
	}

	Write_DAP(DPACC_DP_SELECT_WRITE, 0x00000000);

	if (swd_PacketAck != SWD_OK_ACK) {
		input_dbg(true, g_sd->dev, "%s:%d: swd_PacketAck:%x\n", __func__, __LINE__, swd_PacketAck);
		goto testmode_fail;	//return(FAILURE);
	}

	Write_DAP(DPACC_AP_CSW_WRITE, 0x00000002);

	if (swd_PacketAck != SWD_OK_ACK) {
		input_dbg(true, g_sd->dev, "%s:%d: swd_PacketAck:%x\n", __func__, __LINE__, swd_PacketAck);
		goto testmode_fail;	//return(FAILURE);
	}

	/* Enter CPU into Test Mode */
	Write_IO(TEST_MODE, 0x80000000);

	if (swd_PacketAck != SWD_OK_ACK) {
		input_dbg(true, g_sd->dev, "%s:%d: swd_PacketAck:%x\n", __func__, __LINE__, swd_PacketAck);
		goto testmode_fail;	//return(FAILURE);
	}

	Read_IO(TEST_MODE, &status);

	if (swd_PacketAck != SWD_OK_ACK) {
		input_dbg(true, g_sd->dev, "%s:%d: swd_PacketAck:%x\n", __func__, __LINE__, swd_PacketAck);
		goto testmode_fail;	//return(FAILURE);
	}

	if ((status & 0x80000000) != 0x80000000) {
		input_dbg(true, g_sd->dev, "%s:%d: status:%x\n", __func__, __LINE__, status);
		goto testmode_fail;	//return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		input_dbg(true, g_sd->dev, "%s:%d: result:%x\n", __func__, __LINE__, result);
		goto testmode_fail;	//return(FAILURE);
	}

#if (CY8C40xx_FAMILY)
	/* Set IMO to 48 MHz */
	SetIMO48MHz();

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		input_err(true, g_sd->dev, "%s:%d: fail result:%x\n", __func__, __LINE__, result);
		return (FAILURE);
	}
#endif
	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);

testmode_fail:
	return (FAILURE);
}

#ifdef HEX_FIRMWARE
static u8 VerifySiliconId(void)
{
	u32 deviceSiliconID;
	u32 hexSiliconId = 0;

	u32 parameter1 = 0;
	u32 siliconIdData1 = 0;
	u32 siliconIdData2 = 0;

	input_info(true, g_sd->dev, "%s: \n", __func__);

	/* Read and store Silicon ID from HEX file to hexSiliconId array */
	HEX_ReadSiliconId(&hexSiliconId);

	/* Load Parameter1 with the SROM command to read silicon ID from PSoC 4
	   chip */
	parameter1 = (u32) (((u32) SROM_KEY1 << 0) +	//
			(((u32) SROM_KEY2 +
			  (u32) SROM_CMD_GET_SILICON_ID) << 8));

	/* Load CPUSS_SYSARG register with parameter1 */
	Write_IO(CPUSS_SYSARG, parameter1);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_GET_SILICON_ID);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();
	if (result != SROM_SUCCESS) {
		return (FAILURE);
	}

	/* Read CPUSS_SYSARG and CPUSS_SYSREQ to read 4 bytes of silicon ID */
	Read_IO(CPUSS_SYSARG, &siliconIdData1);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	Read_IO(CPUSS_SYSREQ, &siliconIdData2);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/*
	   SiliconIdData2 (0th byte) = 4th byte of Device Silicon ID (MSB)
	   SiliconIdData1 (3rd byte) = 3rd byte of Device Silicon ID
	   SiliconIdData1 (1st byte) = 2nd byte of Device Silicon ID
	   SiliconIdData1 (2nd byte) = 1st byte of Device Silicon ID (LSB)
	 */
	deviceSiliconID = (((siliconIdData2 << 24) & 0xFF000000) + (siliconIdData1 & 0x00FF0000) +	//
			((siliconIdData1 << 8) & 0x0000FF00) +
			((siliconIdData1 >> 8) & 0x000000FF));
	input_dbg(true, g_sd->dev, "%s: deviceSiliconID=0x%08x\n", __func__,
			deviceSiliconID);
	input_dbg(true, g_sd->dev, "%s: hexSiliconId=0x%08x\n", __func__,
			hexSiliconId);
	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}
#endif //HEX_FIRMWARE

static u8 EraseAllFlash(void)
{
	u32 parameter1 = 0;

	input_dbg(true, g_sd->dev, "%s: \n", __func__);

#ifdef HEX_FIRMWARE
	/* Get current chip protection setting */
	GetChipProtectionVal();

	/* Check if the Chip protection setting transition is valid */
	result = GetTransitionMode();
	if (result != SUCCESS) {
		return (FAILURE);
	}
#else
	flow = OPEN_XXX;
#endif
	/* If the transition is from open to any protection setting or from virgin to
	   open, call ERASE_ALL SROM command */
	if ((flow == OPEN_XXX) || (flow == VIRGIN_OPEN)) {
		parameter1 = (u32) (((u32) SROM_KEY1 << 0) +	//
				(((u32) SROM_KEY2 +
				  (u32) SROM_CMD_ERASE_ALL) << 8));

		/* Load ERASE_ALL SROM command in parameter1 to SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);
		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

		/* Set location of parameters */
		Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);
		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

		/* Request SROM call */
		Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_ERASE_ALL);
		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

		/* Read status of the operation */
		result = PollSromStatus();
		if (result != SUCCESS) {
			return (FAILURE);
		}
		input_info(true, g_sd->dev, "%s: erase all succeeded\n", __func__);
	}

	/* If the transition is from protected mode to open mode or protected mode to
	   protected mode only, call ERASE_ALL SROM command */
	else if (flow == PROT_XXX) {
		/* Move chip to open state: 0x01 corresponds to open state, 0x00 to
		   macro 1 */
		parameter1 = ((u32) SROM_KEY1 << 0) +	//
			(((u32) SROM_KEY2 + (u32) SROM_CMD_WRITE_PROTECTION) << 8) +	//
			(0x01 << 16) + (0x00 << 24);

		/* Load the write protection command to SRAM */
		Write_IO(CPUSS_SYSARG, parameter1);
		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

		/* Request SROM call */
		Write_IO(CPUSS_SYSREQ,
				SROM_SYSREQ_BIT | SROM_CMD_WRITE_PROTECTION);
		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

		/* Read status of the operation */
		result = PollSromStatus();
		if (result != SUCCESS) {
			return (FAILURE);
		}

		/* Re-acquire chip in OPEN mode */
		result = DeviceAcquire();
		if (result != SUCCESS) {
			return (FAILURE);
		}
		input_info(true, g_sd->dev, "%s: move to open state succeeded\n",
				__func__);
	}

	input_dbg(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}

static u8 ChecksumPrivileged(void)
{
	input_dbg(true, g_sd->dev, "%s: \n", __func__);
	result = ChecksumAPI(CHECKSUM_ENTIRE_FLASH, &checksum_Privileged);
	if (result != SUCCESS) {
		return (FAILURE);
	}
	input_info(true, g_sd->dev, "%s: checksum_Privileged=0x%08x\n", __func__,
			checksum_Privileged);
	input_dbg(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}

static u8 ProgramFlash(void)
{
	u8 arrayID = 0;
	u8 rowData[BYTES_PER_FLASH_ROW];

	u16 numOfFlashRows = 0;
	u16 rowCount = 0;

	u32 parameter1 = 0;

	input_info(true, g_sd->dev, "%s: \n", __func__);

	/* Get the total number of flash rows in the Target PSoC 4 device */
	numOfFlashRows = GetFlashRowCount();
	input_dbg(true, g_sd->dev, "%s: numOfFlashRows=%d\n", __func__,
			numOfFlashRows);

	/* Program all flash rows */
	for (rowCount = 0; rowCount < numOfFlashRows; rowCount++) {
#ifdef HEX_FIRMWARE
		HEX_ReadRowData(rowCount, &rowData[0]);
#else
		memcpy(&rowData[0],
				g_sd->fw_img + (rowCount * BYTES_PER_FLASH_ROW),
				BYTES_PER_FLASH_ROW);
#endif
		if (rowCount == 0)
			input_dbg(true, g_sd->dev,
					"%s: row%d file=0x%02x 0x%02x 0x%02x 0x%02x\n",
					__func__, rowCount, rowData[0], rowData[1],
					rowData[2], rowData[3]);
		else if (rowCount == (numOfFlashRows - 1))
			input_dbg(true, g_sd->dev,
					"%s: row%d file=0x%02x 0x%02x 0x%02x 0x%02x\n",
					__func__, rowCount, rowData[0], rowData[1],
					rowData[2], rowData[3]);

		arrayID = rowCount / ROWS_PER_ARRAY;

		result = LoadLatch(arrayID, &rowData[0]);

		if (result != SUCCESS) {
			input_err(true, g_sd->dev, "%s: fail load latch\n", __func__);
			return (FAILURE);
		}

		/* Load parameter1 with Program Row - SROM command */
		parameter1 = (u32) (((u32) SROM_KEY1 << 0) +
				(((u32) SROM_KEY2 +
				  (u32) SROM_CMD_PROGRAM_ROW) << 8) +
				(((u32) rowCount & 0x000000FF) << 16) +
				(((u32) rowCount & 0x0000FF00) << 16));

		/* Write parameters in SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);

		if (swd_PacketAck != SWD_OK_ACK) {
			input_err(true, g_sd->dev, "%s: fail prog row\n", __func__);
			return (FAILURE);
		}

		/* Set location of parameters */
		Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);

		if (swd_PacketAck != SWD_OK_ACK) {
			input_err(true, g_sd->dev, "%s: fail set location\n", __func__);
			return (FAILURE);
		}

		/* Request SROM operation */
		Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_PROGRAM_ROW);

		if (swd_PacketAck != SWD_OK_ACK) {
			input_err(true, g_sd->dev, "%s: fail request SROM operation\n",
					__func__);
			return (FAILURE);
		}

		/* Read status of the operation */
		result = PollSromStatus();

		if (result != SROM_SUCCESS) {
			input_err(true, g_sd->dev, "%s: fail read status\n", __func__);
			return (FAILURE);
		}

	}

	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}

static u8 VerifyFlash(void)
{
	u32 flashData = 0;
	u16 numOfFlashRows = 0;
	u16 rowAddress = 0;
	u16 rowCount;
	u8 i;
	u8 rowData[BYTES_PER_FLASH_ROW];
	u8 chipdata[BYTES_PER_FLASH_ROW];

	input_dbg(true, g_sd->dev, "%s: \n", __func__);

	/* Get the total number of flash rows in the Target PSoC 4 device */
	numOfFlashRows = GetFlashRowCount();
	input_info(true, g_sd->dev, "%s: numOfFlashRows=%d\n", __func__,
			numOfFlashRows);

	/* Read and Verify Flash rows */
	for (rowCount = 0; rowCount < numOfFlashRows; rowCount++) {
		/* Read row from hex file */

		/* linear address of row in flash */
		rowAddress = BYTES_PER_FLASH_ROW * rowCount;

		/* Extract 128-byte row from the hex-file from address: ?�rowCount??into
		   buffer - ?�rowData?? */
#ifdef HEX_FIRMWARE
		HEX_ReadRowData(rowCount, &rowData[0]);
#else
		memcpy(&rowData[0],
				g_sd->fw_img + (rowCount * BYTES_PER_FLASH_ROW),
				BYTES_PER_FLASH_ROW);
#endif

		if (rowCount == 0)
			input_dbg(true, g_sd->dev,
					"%s: row%d file=0x%02x 0x%02x 0x%02x 0x%02x\n",
					__func__, rowCount, rowData[0], rowData[1],
					rowData[2], rowData[3]);
		else if (rowCount == (numOfFlashRows - 1))
			input_dbg(true, g_sd->dev,
					"%s: row%d file=0x%02x 0x%02x 0x%02x 0x%02x\n",
					__func__, rowCount, rowData[0], rowData[1],
					rowData[2], rowData[3]);

		/* Read row from chip */
		for (i = 0; i < BYTES_PER_FLASH_ROW; i += 4) {
			/* Read flash via AHB-interface */
			Read_IO(rowAddress + i, &flashData);
			if (swd_PacketAck != SWD_OK_ACK) {
				return (FAILURE);
			}

			chipdata[i + 0] = (flashData >> 0) & 0xFF;
			chipdata[i + 1] = (flashData >> 8) & 0xFF;
			chipdata[i + 2] = (flashData >> 16) & 0xFF;
			chipdata[i + 3] = (flashData >> 24) & 0xFF;
		}
		if (rowCount == 0)
			input_dbg(true, g_sd->dev,
					"%s: row%d chip=0x%02x 0x%02x 0x%02x 0x%02x\n",
					__func__, rowCount, chipdata[0], chipdata[1],
					chipdata[2], chipdata[3]);

		else if (rowCount == (numOfFlashRows - 1))
			input_dbg(true, g_sd->dev,
					"%s: row%d chip=0x%02x 0x%02x 0x%02x 0x%02x\n",
					__func__, rowCount, chipdata[0], chipdata[1],
					chipdata[2], chipdata[3]);

		/* Compare the row data of HEX file with chip data */
		for (i = 0; i < BYTES_PER_FLASH_ROW; i++) {
			if (chipdata[i] != rowData[i]) {
				swd_PacketAck =
					swd_PacketAck | VERIFICATION_ERROR;
				return (FAILURE);
			}
		}
	}
	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}

#if defined(HEX_FIRMWARE) || defined(FORCE_PROTECT)
static u8 ProgramProtectionSettings(void)
{
	u8 arrayID = 0;
	u8 rowProtectionByteSize = 0;
	u8 rowProtectionData[MAXIMUM_ROW_PROTECTION_BYTE_LENGTH];
	u8 chipProtectionData_Hex;

	u16 numOfFlashRows = 0;

	u32 parameter1 = 0;

	input_info(true, g_sd->dev, "%s: \n", __func__);

	/* Get total number of flash rows to determine the size of row protection data
	   and arrayID */
	numOfFlashRows = GetFlashRowCount();
	input_dbg(true, g_sd->dev, "%s: numOfFlashRows=%d\n", __func__,
			numOfFlashRows);

	rowProtectionByteSize = numOfFlashRows / 8;
	input_dbg(true, g_sd->dev, "%s: rowProtectionByteSize=%d\n", __func__,
			rowProtectionByteSize);

	arrayID = numOfFlashRows / ROWS_PER_ARRAY;
	input_dbg(true, g_sd->dev, "%s: arrayID=%d\n", __func__, arrayID);

	HEX_ReadChipProtectionData(&chipProtectionData_Hex);
	input_dbg(true, g_sd->dev, "%s: chipProtectionData=%d\n", __func__,
			chipProtectionData_Hex);

	HEX_ReadRowProtectionData(rowProtectionByteSize, &rowProtectionData[0]);
	input_dbg(true, g_sd->dev, "%s: rowProtectionData0~2:0x%02x 0x%02x 0x%02x \n",
			__func__, rowProtectionData[0], rowProtectionData[1],
			rowProtectionData[2]);

	/* Load protection setting of current macro into volatile latch using
	   LoadLatch API */
	result =
		LoadLatch_protect(arrayID, &rowProtectionData[0],
				rowProtectionByteSize);

	if (result != SUCCESS) {
		return (FAILURE);
	}

	/* Program protection setting into supervisory row */
	parameter1 = (u32) (((u32) SROM_KEY1 << 0) +	//
			(((u32) SROM_KEY2 + (u32) SROM_CMD_WRITE_PROTECTION) << 8) +	//
			((u32) chipProtectionData_Hex << 16));

	/* Load parameter1 in CPUSS_SYSARG register */
	Write_IO(CPUSS_SYSARG, parameter1);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Request SROM call */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_WRITE_PROTECTION);

	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SROM_SUCCESS) {
		return (FAILURE);
	}

	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}

static u8 VerifyProtectionSettings(void)
{
	u32 protectionData = 0;
	u32 flashProtectionAddress = 0;
	u16 numOfFlashRows = 0;
	u8 chipProtectionData_Hex = 0;
	u8 rowProtectionByteSize = 0;
	u8 i;
	u8 rowProtectionData[MAXIMUM_ROW_PROTECTION_BYTE_LENGTH];
	u8 rowProtectionFlashData[MAXIMUM_ROW_PROTECTION_BYTE_LENGTH];

	input_info(true, g_sd->dev, "%s: \n", __func__);

	numOfFlashRows = GetFlashRowCount();

	rowProtectionByteSize = numOfFlashRows / 8;
	input_dbg(true, g_sd->dev, "%s: rowProtectionByteSize=%d\n", __func__,
			rowProtectionByteSize);

	flashProtectionAddress = SFLASH_MACRO;

	/* Read Protection settings from hex-file */
	HEX_ReadRowProtectionData(rowProtectionByteSize, &rowProtectionData[0]);

	/* Read Protection settings from silicon */
	for (i = 0; i < rowProtectionByteSize; i += 4) {
		Read_IO(flashProtectionAddress + i, &protectionData);

		if (swd_PacketAck != SWD_OK_ACK) {
			return (FAILURE);
		}

		rowProtectionFlashData[i + 0] = (protectionData >> 0) & 0xFF;
		rowProtectionFlashData[i + 1] = (protectionData >> 8) & 0xFF;
		rowProtectionFlashData[i + 2] = (protectionData >> 16) & 0xFF;
		rowProtectionFlashData[i + 3] = (protectionData >> 24) & 0xFF;

		if (i == 0)
			input_dbg(true, g_sd->dev,
					"%s: rowProtectionFlashData0~2:0x%02x 0x%02x 0x%02x \n",
					__func__, rowProtectionData[0],
					rowProtectionData[1], rowProtectionData[2]);
	}

	/* Compare hex and silicon?�s data */
	for (i = 0; i < rowProtectionByteSize; i++) {
		if (rowProtectionData[i] != rowProtectionFlashData[i]) {
			/* Set the verification error bit for Flash protection data
			   mismatch and return failure */
			swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
			return (FAILURE);
		}
	}

	/* Read Chip Level Protection from hex-file */
	HEX_ReadChipProtectionData(&chipProtectionData_Hex);
	input_dbg(true, g_sd->dev, "%s: chipProtectionData=%d\n", __func__,
			chipProtectionData_Hex);

	/* Read Chip Level Protection from the silicon */
	Read_IO(SFLASH_CPUSS_PROTECTION, &protectionData);
	if (swd_PacketAck != SWD_OK_ACK) {
		return (FAILURE);
	}

	chipProtectionData_Chip = (protectionData >> 24) & 0x0F;
	input_dbg(true, g_sd->dev, "%s: chipProtectionData_Chip=0x%02x\n", __func__,
			chipProtectionData_Chip);

	if (chipProtectionData_Chip == CHIP_PROT_VIRGIN) {
		chipProtectionData_Chip = CHIP_PROT_OPEN;
		//input_dbg(true, g_sd->dev, "%s: chip protection open\n", __func__);
	} else if (chipProtectionData_Chip == CHIP_PROT_OPEN) {
		chipProtectionData_Chip = CHIP_PROT_VIRGIN;
		//input_dbg(true, g_sd->dev, "%s: chip protection virgin\n", __func__);
	}

	/* Compare hex?�s and silicon?�s chip protection data */
	if (chipProtectionData_Chip != chipProtectionData_Hex) {
		/* Set the verification error bit for Flash protection data
		   mismatch and return failure */
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		return (FAILURE);
	}

	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}
#endif

static u8 VerifyChecksum(void)
{
	u32 checksum_All = 0;
	u16 chip_Checksum = 0;

	input_info(true, g_sd->dev, "%s: \n", __func__);

	/* Read the checksum of entire flash */
	result = ChecksumAPI(CHECKSUM_ENTIRE_FLASH, &checksum_All);

	if (result != SUCCESS) {
		input_err(true, g_sd->dev, "%s: fail ChecksumAPI\n", __func__);
		return (FAILURE);
	}
	input_dbg(true, g_sd->dev, "%s: checksum of entire flash=0x%08x\n", __func__,
			checksum_All);

	/* Calculate checksum of user flash */
	chip_Checksum = (u16) checksum_All - (u16) checksum_Privileged;
	input_info(true, g_sd->dev, "%s: chip_Checksum=0x%08x\n", __func__,
			chip_Checksum);
	input_info(true, g_sd->dev, "%s: checksum from fw file=0x%08x\n", __func__,
			g_sd->swd_tkey_i2c->fw_img->checksum);

	/* Compare the checksum data of silicon and hex file */
	if (chip_Checksum != g_sd->swd_tkey_i2c->fw_img->checksum) {
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		input_err(true, g_sd->dev, "%s: fail compare checksum\n", __func__);
		return (FAILURE);
	}

	input_info(true, g_sd->dev, "%s: success\n", __func__);
	return (SUCCESS);
}

static void ExitProgrammingMode(void)
{
	input_info(true, g_sd->dev, "%s: \n", __func__);

	/* Generate active low rest pulse for 100 uS */
	SetXresLow();
	msleep(20);
	SetXresHigh();
}

static u8 ProgramDevice(struct swd_data *sd)
{
	if (DeviceAcquire() == FAILURE)
		goto fail;
#if !defined(CRC_CHECK_INTERNAL)
	if (sd->swd_tkey_i2c->do_checksum == true) {
		if (VerifyChecksum() == SUCCESS) {
			input_info(true, sd->dev, "%s: fw update pass\n", __func__);
			goto out;
		}
	}
#endif
#ifdef HEX_FIRMWARE
	if (VerifySiliconId() == FAILURE)
		goto fail;
#endif
	if (EraseAllFlash() == FAILURE)
		goto fail;

	if (ChecksumPrivileged() == FAILURE)
		goto fail;

	if (ProgramFlash() == FAILURE)
		goto fail;

	if (VerifyFlash() == FAILURE)
		goto fail;
#if defined(HEX_FIRMWARE) || defined(FORCE_PROTECT)
	if (ProgramProtectionSettings() == FAILURE)
		goto fail;

	if (VerifyProtectionSettings() == FAILURE)
		goto fail;
#endif
	if (VerifyChecksum() == FAILURE)
		goto fail;

#if !defined(CRC_CHECK_INTERNAL)
out:
#endif
	ExitProgrammingMode();
	/* All the steps were completed successfully */
	return (SUCCESS);
fail:
	ExitProgrammingMode();
	return (FAILURE);
}

/*****************************************************************************
 * probe
 *****************************************************************************/
int cy8cmbr_swd_program(struct touchkey_i2c *tkey_i2c)
{
	struct swd_data *sd;
	int rc;

	input_info(true, &tkey_i2c->client->dev, "%s: \n", __func__);

	memset(&_swd_data, 0, sizeof(_swd_data));
	sd = &_swd_data;
	sd->swd_tkey_i2c = tkey_i2c;
	sd->power = tkey_i2c->power;
	sd->swdio_gpio = tkey_i2c->dtdata->gpio_sda;
	sd->swdck_gpio = tkey_i2c->dtdata->gpio_scl;
	sd->fw_img = tkey_i2c->fw_img->data;
	sd->fw_img_size = tkey_i2c->fw_img->fw_len;
	sd->dev = &tkey_i2c->client->dev;
	g_sd = sd;

	Swd_configPhysical(sd->swdio_gpio, sd->swdck_gpio);
	if (ProgramDevice(sd) == SUCCESS)
		rc = 0;
	else
		rc = -1;

	input_info(true, &tkey_i2c->client->dev, "%s: end, rc=%d\n", __func__, rc);
	return rc;
}
