#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/i2c-dev.h>
#include <errno.h>

//#define DEBUG

#define ConfigurationRegisterA 0x00
#define ConfigurationRegisterB 0x01
#define ModeRegister 0x02
#define AxisXDataRegisterMSB 0x03
#define AxisXDataRegisterLSB 0x04
#define AxisZDataRegisterMSB 0x05
#define AxisZDataRegisterLSB 0x06
#define AxisYDataRegisterMSB 0x07
#define AxisYDataRegisterLSB 0x08
#define StatusRegister 0x09
#define IdentificationRegisterA 0x10
#define IdentificationRegisterB 0x11
#define IdentificationRegisterC 0x12

#define MeasurementContinuous 0x00
#define MeasurementSingleShot 0x01
#define MeasurementIdle = 0x03

static int scale_reg;
static float scale;
static int i2c_fd;
static int i2cbus_port;
static uint16_t magn_addr;

int twoComplCalc(int val, int len) {
  if(val & (1 << len - 1)) {
    val = val - (1<<len);
  }
  return val;
}

void __i2c_transaction(int cnt, ...) {
  struct i2c_rdwr_ioctl_data i2c_data;
  struct i2c_msg* msgs = (struct i2c_msg*)malloc(cnt * sizeof(struct i2c_msg));

  va_list arg_list;
  va_start(arg_list, cnt);
  for (int i = 0; i < cnt; ++i) {
    msgs[i] = va_arg(arg_list, struct i2c_msg);
  }
  va_end(arg_list);

  i2c_data.msgs = msgs;
  i2c_data.nmsgs = cnt;
  printf("i2c_data's msg address: %x\n", i2c_data.msgs->addr);
  if (ioctl(i2c_fd, I2C_RDWR, &i2c_data) < 0) {
    fprintf(stderr, "transaction fail(ioctl), %s\n", strerror(errno));
  }
}

struct i2c_msg __new_i2c_msg(unsigned short flags, int len, char* buffer) {
  struct i2c_msg msg;
  msg.flags = flags;
  msg.addr = magn_addr;
  msg.len = len;
  msg.buf = buffer;

#ifdef DEBUG
  unsigned int data, d; data = 0;
  for (int i = 0; i < len; ++i) {
    d = msg.buf[i];
    printf("buffer seg: %d: %x\n", i, d);
    data += d << (2*(len-1-i)*4);
  }
  printf("msg buffer: %x\n", data);
#endif

  return msg;
}

struct i2c_msg i2c_rd_msg(int buf_len, char* buffer) {
  struct i2c_msg msg = __new_i2c_msg(I2C_M_RD, buf_len, buffer);
  return msg;
}

struct i2c_msg i2c_wrt_msg(int buf_len, char* buffer) {
  struct i2c_msg msg = __new_i2c_msg(0, buf_len, buffer);
  return msg;
}

void i2c_write_byte(int cnt, ...) {
  char* buffer = (char*) malloc(cnt*sizeof(char));
  va_list arg_list;
  va_start(arg_list, cnt);
  for (int i = 0; i < cnt; ++i) {
    buffer[i] = (char)va_arg(arg_list, int);
  }
  va_end(arg_list);
  struct i2c_msg wrt_msg = i2c_wrt_msg(cnt, buffer);
  __i2c_transaction(1, wrt_msg);
  free(buffer);
}

void set_option(int reg, int cnt, ...) {
  int options; options=0x00;
  va_list opt_list;
  va_start(opt_list, cnt);
  for (int i = 0; i < cnt; ++i) {
    options |= va_arg(opt_list, int);
  }
  va_end(opt_list);
  i2c_write_byte(2, reg, options);
}

void set_scale() {
  //temporary setup
  scale_reg = 1 << 5;
  //scale_reg = scale_reg << 5;
  set_option(ConfigurationRegisterB, 1, scale_reg);
}

void set_continuous_mode() {
  set_option(ModeRegister, 1, MeasurementContinuous);
}

void getAxes() {
  char reg = (char)AxisXDataRegisterMSB;
  struct i2c_msg  wrt_msg = i2c_wrt_msg(1, &reg);
  char read[6] = {0};
  struct i2c_msg rd_msg = i2c_rd_msg(6, read);
  __i2c_transaction(2, wrt_msg, rd_msg);
#ifdef DEBUG
  for (int i = 0; i < 6; ++i) {
    printf("read %d: %d; ", i, read[i]);
  }
  printf("\n");
#endif
  int read_x = read[1];
  read_x |= (read[0] << 8);
  read_x = twoComplCalc(read_x, 16);
  int read_y = read[3];
  read_y |= (read[2]  << 8);
  read_y = twoComplCalc(read_y, 16);
  int read_z = read[5];
  read_z |= (read[4] << 8);
  read_z = twoComplCalc(read_z, 16);
  printf("getAxes: x=%d, y=%d, z=%d\n", read_x, read_y, read_z);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage:  %s <i2c-bus> <i2c-address>\n", argv[0]);
    exit(1);
  }

  i2cbus_port = atoi(argv[1]);
  magn_addr = strtol(argv[2], NULL, 16);

  char i2c_dev_name[32];
  sprintf(i2c_dev_name, "/dev/i2c-%d", i2cbus_port);
  i2c_fd = open(i2c_dev_name, O_RDWR);
  if (i2c_fd < 0) {
    fprintf(stderr, "i2c-dev open fail\n");
    exit(1);
  }
  if (ioctl(i2c_fd, I2C_SLAVE, magn_addr) < 0) {
    fprintf(stderr, "access slave addr fail(ioctl), %s\n", strerror(errno));
    exit(1);
  }
  set_scale();
  set_continuous_mode();
  usleep(6000);
  getAxes();

  return 0;
}
