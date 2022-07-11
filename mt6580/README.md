# MT6580

## General specs

  - ARM Cortex-R4 r1p4 (MIDR == 0x411fc144)
  - 256k I/D CTMs

## Memory map

|  Address   |  Length  |                  Description                  |
|------------|----------|-----------------------------------------------|
| 0x00000000 | 256 MiB  | "Bank0" remappable area (executable)          |
| 0x10000000 | 768 MiB  | Maps directly to EMI 0x10000000+ (executable) |
| 0x40000000 | 256 MiB  | "Bank4" remappable area (non-executable)      |
| 0x80000000 | 256+ MiB | Modem peripherals                             |
| 0xA0000000 | 256+ MiB | AP peripherals                                |

### Bank0/Bank4 areas

These areas contain 8 regions by 32 MiB each that can be remapped to the corresponding 32 MiB block in the EMI.

This is controlled in the `INFRACFG_AO` (`0x10001000`) regs `0x300`/`0x304` (for Bank0) and `0x308`/`0x30C` (for Bank1).

Each byte corresponds to the 32 MiB region in the Bank0/Bank4 (i.e. `0x300` is for `0x00000000~0x01ffffff`, `0x301` is for `0x02000000~0x03ffffff` and etc.)

The LSB indicates whether this region is enabled or not, the remaining bits is the EMI address bits 25..31.

E.g. `0x300 = 0x07050301` and `0x304 = 0x1f141211` will create the following mapping:

|    Modem     |     EMI      |
|--------------|--------------|
| `0x00000000` | `0x00000000` |
| `0x02000000` | `0x02000000` |
| `0x04000000` | `0x04000000` |
| `0x06000000` | `0x06000000` |
| `0x08000000` | `0x10000000` |
| `0x0A000000` |   disabled   |
| `0x0C000000` |   disabled   |
| `0x0E000000` | `0x1E000000` |

## AP Peripherals

These are most related to the bringup and operation of the Modem

|  AP address  |  MD address  |       Name       |        Description        |
|--------------|--------------|------------------|---------------------------|
| `0x10001000` | `0xA0001000` | `INFRACFG_AO`    | Infrastructure config     |
| `0x10006000` | `0xA0006000` | `SLEEP` or `SPM` | System power management   |
| `0x1000C000` | `0xA000C000` | `AP_CCIF0`       | AP CPU-CPU Interface      |
| `0x1000D000` | `0xA000D000` | `MD_CCIF0`       | MD CPU-CPU Interface      |
| `0x10205000` | `0xA0205000` | `EMI`            | External memory interface |

## Modem Peripherals

|   MD address  |  AP address  |     Name      |            Description             |
|---------------|--------------|---------------|------------------------------------|
| `0x80000000`  | `0x20000000` | `MD_MCU`      |                                    |
| `0x80030000`  | `0x20030000` | `MD_TOPSM`    |                                    |
| `0x80040000`  | `0x20040000` | `MD_OS_TIMER` |                                    |
| `0x80050000`  | `0x20050000` | `MD_RGU`      | "Reset Generation Unit" + Watchdog |
| `0x80070000`  |      n/a     |  UART         | Modem's UART                       |
| `0x80120000`  | `0x20120000` | `MD_MIXEDSYS` |                                    |
| `0x80190000`? | `0x20190000` |  ???          | Modem CPU bringup thing            |
| `0x83000000`? | `0x23000000` | `M3D`         |                                    |
| `0x83010000`? | `0x23010000` | `MODEM_TOPSM` |                                    |
| `0x84000000`? | `0x24000000` | `TDD`         |                                    |

## See also

-  [How to bringup](bringup.md)
-  [How to use (the firmware blob)](usage.md)
