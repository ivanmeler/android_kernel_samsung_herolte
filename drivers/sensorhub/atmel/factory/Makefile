#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP_MPU6500)	+= accel_mpu6500.o gyro_mpu6500.o
obj-$(CONFIG_SENSORS_SSP_BMI058)	+= accel_bmi058.o gyro_bmi058.o
obj-$(CONFIG_SENSORS_SSP_YAS532)	+= magnetic_yas532.o

obj-$(CONFIG_SENSORS_SSP_CM3320)	+= light_cm3320.o
obj-$(CONFIG_SENSORS_SSP_MAX88920)	+= prox_max88920.o gesture_max88920.o
obj-$(CONFIG_SENSORS_SSP_MAX88921)	+= light_max88921.o prox_max88920.o gesture_max88922.o

obj-$(CONFIG_SENSORS_SSP_TMG399X)	+= light_tmg399x.o prox_max88920.o gesture_tmg399x.o
obj-$(CONFIG_SENSORS_SSP_BMP182)	+= pressure_bmp182.o
obj-$(CONFIG_SENSORS_SSP_LPS25H)	+= pressure_lps25h.o

obj-$(CONFIG_SENSORS_SSP_AT32UC3L0128)	+= mcu_at32uc3l0128.o
obj-$(CONFIG_SENSORS_SSP_ATUC128L5HAR)	+= mcu_atuc128l5har.o
obj-$(CONFIG_SENSORS_SSP_SHTC1)	+= temphumidity_shtc1.o

obj-$(CONFIG_SENSORS_SSP_MOBEAM)	 += barcode_emul_tmg3992.o

