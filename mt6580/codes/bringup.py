from periphery import MMIO, sleep_ms
import sys, time

infracfg_ao = MMIO(0x10001000, 0x1000)
sleep       = MMIO(0x10006000, 0x1000)
ap_ccif0    = MMIO(0x1000c000, 0x1000)
emi         = MMIO(0x10205000, 0x1000)

md1_md_rgu  = MMIO(0x20050000, 0x10000)
md_20190000 = MMIO(0x20190000, 0x10000)

konata      = MMIO(0x88000000, 0x8000000)  #<< Marked as Reserved memory in Device tree

## en sleep regs control ##
sleep.write32(0x000, 0x0B160001)

# Load our super firmware
with open(sys.argv[1], 'rb') as f:
    konata.write(0, f.read())

########################################

def md_power_ctl(en):
    if en:
        ## turn on md ##
        sleep.write32(0x284, sleep.read32(0x284) | (1<<2))    #<< set pwr_on
        sleep.write32(0x284, sleep.read32(0x284) | (1<<3))    #<< set pwr_on_2nd
        sleep.write32(0x284, sleep.read32(0x284) & ~(1<<4))   #<< clr pwr_clk_dis
        sleep.write32(0x284, sleep.read32(0x284) & ~(1<<1))   #<< clr pwr_iso
        sleep.write32(0x284, sleep.read32(0x284) | (1<<0))    #<< set pwr_rst_b
        sleep.write32(0x284, sleep.read32(0x284) & ~(0x1<<8)) #<< clr sram_pdn

        ## disable AXI protection ##
        infracfg_ao.write32(0x220, infracfg_ao.read32(0x220) & ~0xcc)
    else:
        ## enable AXI protection ##
        infracfg_ao.write32(0x220, infracfg_ao.read32(0x220) | 0xcc)

        ## turn off md ##
        sleep.write32(0x284, sleep.read32(0x284) | (0x1<<8))           #<< set sram_pdn
        sleep.write32(0x284, (sleep.read32(0x284) & ~(1<<0)) | (1<<1)) #<< clr pwr_rst_b, set pwr_iso
        sleep.write32(0x284, sleep.read32(0x284) | (1<<4))             #<< set pwr_clk_dis
        sleep.write32(0x284, sleep.read32(0x284) & ~((1<<2)|(1<<3)))   #<< clr pwr_on, pwr_on_2nd

md_power_ctl(False)

#======== Setup modem mapping =========#

'''
00000000~0fffffff => Bank0 (read,write,execute)
10000000~3fffffff ====> ??? [Maps DRAM 10000000~3fffffff] (read,write,execute)
40000000~4fffffff => Bank4 (read,write)
80000000~8fffffff => Modem peripherals [AP range: 20000000~2fffffff] --though not everyhing is accessible from AP side
A0000000~Afffffff => AP peripherals    [AP range: 10000000~1fffffff]
'''

# modem bank0 "rom" ===> Map of 0x00000000~0x0FFFFFFF of the Modem MCU [Cortex-R4F]
infracfg_ao.write32(0x300, 0x01010101 | 0x06040200 | ((konata.base-0x80000000)>>24))
infracfg_ao.write32(0x304, 0x01010101 | 0x0e0c0a08)

# modem bank4 "shared mem" ===> Map of 0x40000000~0x4FFFFFFF of the Modem MCU [Cortex-R4F]
infracfg_ao.write32(0x308, 0x00000000 | 0x00000408)
infracfg_ao.write32(0x30C, 0x00000000 | 0x00000000)

#======= Disable MPU against MD ======#
'''
grp0 = "secure os"
grp1 = "md0 rom"
grp2 = "md0 r/w+"
grp3 = "md0 share"
grp4 = "connsys code"
grp5 = "ap-connsys? share"
grp6 = "ap"
grp7 = "ap"
---the first group has higher priority than the folloeing

dev0 = AP
dev1 = MD0
dev2 = CONNSYS
dev3 = MM

0 = No protection
1 = Secure R/W only
2 = Secure R/W and nonsecure R
3 = Secure R/W and nonsecure W
4 = Secure R and nonsecure R
5 = Forbidden
6 = Secure R and nonsecure R/W
'''

# just disable everythong
emi.write32(0x1A0, 0x00000000) # group1, group0
emi.write32(0x1A8, 0x00000000) # group3, group2
emi.write32(0x1B0, 0x00000000) # group5, group4
emi.write32(0x1B8, 0x0b680000) # group7, group6

konarange = (konata.base&0x7fff0000)|(((konata.base+konata.size+0xffff)&0x7fff0000)>>16)
emi.write32(0x160, 0x00000000) # group0
emi.write32(0x168, 0x00000000) # group1
emi.write32(0x170, 0x00000000) # group2
emi.write32(0x178, 0x00000000) # group3
emi.write32(0x180, konarange)  # group4
emi.write32(0x188, 0x00000000) # group5
emi.write32(0x190, 0x00000000) # group6
emi.write32(0x198, 0x00008000) # group7

#========== Power on Modem ===========#
md_power_ctl(True)

#========= Disable Modem WDT ==========#
md1_md_rgu.write32(0x0000, 0x2200)

#======== Start the Modem CPU =========#
md_20190000.write32(0x379c, 0x3567C766) #< key
md_20190000.write32(0x0000, 0x00000000) #< reset (vector) addr
md_20190000.write32(0x5488, 0xA3B66175) #< enable

#======================================#

print("---------------------Entry---------------------")

starttime = time.time()

while True:
    try:
        ## stdout ##
        mbx = ap_ccif0.read32(0x200)
        if mbx & (1<<31):
            ap_ccif0.write32(0x200, 0)
            mbx &= ~(1<<31)
            if (mbx >> 16) == 0x4777:
                print("####### Program exited, rc=%08x #######" % mbx)
                break

            #print(chr(mbx),end='',flush=True)
            sys.stdout.buffer.write(bytes([mbx&0xff]))
            sys.stdout.flush()

        ## stdin ##

        ## realtime ##
        ap_ccif0.write32(0x208, int((time.time()-starttime)*1000000))

    except Exception as e:
        print(e)
        break

    except KeyboardInterrupt:
        break

print("----------------------Exit---------------------")

#========== Power down Modem ===========#
md_power_ctl(False)
