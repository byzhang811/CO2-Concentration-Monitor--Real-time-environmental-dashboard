#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>

#define CCS811_I2C_DEV  "/dev/i2c-2"   
#define CCS811_ADDR     0x5B           

static int fd = -1;
static int inited = 0;

// initialize CCS811（APP_START + MEAS_MODE）
int init_ccs811(void)
{
    if (inited) {
        return 0;
    }

    fd = open(CCS811_I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, CCS811_ADDR) < 0) {
        perror("Failed to set I2C address");
        return -2;
    }

    // -------- APP_START --------
    uint8_t app_start = 0xF4;
    if (write(fd, &app_start, 1) != 1) {
        perror("Failed to send APP_START");
        return -3;
    }
    usleep(10000);  // 10 ms

    // -------- MEAS_MODE = 1s --------
    uint8_t meas_mode[2] = {0x01, 0x10};
    if (write(fd, meas_mode, 2) != 2) {
        perror("Failed to write MEAS_MODE");
        return -4;
    }

    inited = 1;
    return 0;
}

// read eCO2（ppm）
int read_co2_ppm(void)
{
    if (!inited) {
        if (init_ccs811() != 0) {
            return -1;
        }
        sleep(1);  
    }

    uint8_t reg = 0x02;   // ALG_RESULT_DATA
    uint8_t data[4];

    // select register
    if (write(fd, &reg, 1) != 1) {
        perror("write reg");
        return -2;
    }

    // read 4 bytes
    if (read(fd, data, 4) != 4) {
        perror("read data");
        return -3;
    }

    int eCO2 = (data[0] << 8) | data[1];
    return eCO2;
}
