from periphery import MMIO, sleep_ms
import sys, time

infracfg_ao = MMIO(0x10001000, 0x1000)
sleep       = MMIO(0x10006000, 0x1000)
ap_ccif0    = MMIO(0x10209000, 0x1000)
emi_mpu     = MMIO(0x10226000, 0x1000)

konata      = MMIO(0x60000000, 0x8000000)  #<< Marked as Reserved memory in Device tree

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
......
80000000~8fffffff => Modem peripherals [AP range: 20000000~2fffffff] --though not everyhing is accessible from AP side
A0000000~Afffffff => AP peripherals    [AP range: 10000000~1fffffff]
'''

#======= Disable MPU against MD ======#

#========== Power on Modem ===========#
md_power_ctl(True)

#========= Disable Modem WDT ==========#

#======== Start the Modem CPU =========#

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
