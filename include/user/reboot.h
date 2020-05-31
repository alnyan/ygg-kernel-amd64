#pragma once

#define YGG_REBOOT_RESTART      0x01234567
#define YGG_REBOOT_POWER_OFF    0x4321FEDC
#define YGG_REBOOT_HALT         0xCDEF0123

#define YGG_REBOOT_MAGIC1       0xD1ED1ED1
#define YGG_REBOOT_MAGIC2       0x818471ED

#if defined(__KERNEL__)
// TODO: move to kernel-only header
void system_power_cmd(unsigned int cmd);
#else
int reboot(int magic1, int magic2, unsigned int cmd, void *arg);
#endif
