#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/BitLogic.hpp"
#include "infra/util/EnumCast.hpp"

namespace
{
    extern "C" uint32_t SystemCoreClock;

    constexpr const uint32_t PWM_CTL_GLOBALSYNC3 = 0x00000008; // Update PWM Generator 3
    constexpr const uint32_t PWM_CTL_GLOBALSYNC2 = 0x00000004; // Update PWM Generator 2
    constexpr const uint32_t PWM_CTL_GLOBALSYNC1 = 0x00000002; // Update PWM Generator 1
    constexpr const uint32_t PWM_CTL_GLOBALSYNC0 = 0x00000001; // Update PWM Generator 0

    constexpr const uint32_t PWM_SYNC_SYNC3 = 0x00000008; // Reset Generator 3 Counter
    constexpr const uint32_t PWM_SYNC_SYNC2 = 0x00000004; // Reset Generator 2 Counter
    constexpr const uint32_t PWM_SYNC_SYNC1 = 0x00000002; // Reset Generator 1 Counter
    constexpr const uint32_t PWM_SYNC_SYNC0 = 0x00000001; // Reset Generator 0 Counter

    constexpr const uint32_t PWM_ENABLE_PWM7EN = 0x00000080; // MnPWM7 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM6EN = 0x00000040; // MnPWM6 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM5EN = 0x00000020; // MnPWM5 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM4EN = 0x00000010; // MnPWM4 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM3EN = 0x00000008; // MnPWM3 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM2EN = 0x00000004; // MnPWM2 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM1EN = 0x00000002; // MnPWM1 Output Enable
    constexpr const uint32_t PWM_ENABLE_PWM0EN = 0x00000001; // MnPWM0 Output Enable

    constexpr const uint32_t PWM_INVERT_PWM7INV = 0x00000080; // Invert MnPWM7 Signal
    constexpr const uint32_t PWM_INVERT_PWM6INV = 0x00000040; // Invert MnPWM6 Signal
    constexpr const uint32_t PWM_INVERT_PWM5INV = 0x00000020; // Invert MnPWM5 Signal
    constexpr const uint32_t PWM_INVERT_PWM4INV = 0x00000010; // Invert MnPWM4 Signal
    constexpr const uint32_t PWM_INVERT_PWM3INV = 0x00000008; // Invert MnPWM3 Signal
    constexpr const uint32_t PWM_INVERT_PWM2INV = 0x00000004; // Invert MnPWM2 Signal
    constexpr const uint32_t PWM_INVERT_PWM1INV = 0x00000002; // Invert MnPWM1 Signal
    constexpr const uint32_t PWM_INVERT_PWM0INV = 0x00000001; // Invert MnPWM0 Signal

    constexpr const uint32_t PWM_FAULT_FAULT7 = 0x00000080; // MnPWM7 Fault
    constexpr const uint32_t PWM_FAULT_FAULT6 = 0x00000040; // MnPWM6 Fault
    constexpr const uint32_t PWM_FAULT_FAULT5 = 0x00000020; // MnPWM5 Fault
    constexpr const uint32_t PWM_FAULT_FAULT4 = 0x00000010; // MnPWM4 Fault
    constexpr const uint32_t PWM_FAULT_FAULT3 = 0x00000008; // MnPWM3 Fault
    constexpr const uint32_t PWM_FAULT_FAULT2 = 0x00000004; // MnPWM2 Fault
    constexpr const uint32_t PWM_FAULT_FAULT1 = 0x00000002; // MnPWM1 Fault
    constexpr const uint32_t PWM_FAULT_FAULT0 = 0x00000001; // MnPWM0 Fault

    constexpr const uint32_t PWM_INTEN_INTFAULT3 = 0x00080000; // Interrupt Fault 3
    constexpr const uint32_t PWM_INTEN_INTFAULT2 = 0x00040000; // Interrupt Fault 2
    constexpr const uint32_t PWM_INTEN_INTFAULT1 = 0x00020000; // Interrupt Fault 1
    constexpr const uint32_t PWM_INTEN_INTFAULT0 = 0x00010000; // Interrupt Fault 0
    constexpr const uint32_t PWM_INTEN_INTPWM3 = 0x00000008;   // PWM3 Interrupt Enable
    constexpr const uint32_t PWM_INTEN_INTPWM2 = 0x00000004;   // PWM2 Interrupt Enable
    constexpr const uint32_t PWM_INTEN_INTPWM1 = 0x00000002;   // PWM1 Interrupt Enable
    constexpr const uint32_t PWM_INTEN_INTPWM0 = 0x00000001;   // PWM0 Interrupt Enable

    constexpr const uint32_t PWM_RIS_INTFAULT3 = 0x00080000; // Interrupt Fault PWM 3
    constexpr const uint32_t PWM_RIS_INTFAULT2 = 0x00040000; // Interrupt Fault PWM 2
    constexpr const uint32_t PWM_RIS_INTFAULT1 = 0x00020000; // Interrupt Fault PWM 1
    constexpr const uint32_t PWM_RIS_INTFAULT0 = 0x00010000; // Interrupt Fault PWM 0
    constexpr const uint32_t PWM_RIS_INTPWM3 = 0x00000008;   // PWM3 Interrupt Asserted
    constexpr const uint32_t PWM_RIS_INTPWM2 = 0x00000004;   // PWM2 Interrupt Asserted
    constexpr const uint32_t PWM_RIS_INTPWM1 = 0x00000002;   // PWM1 Interrupt Asserted
    constexpr const uint32_t PWM_RIS_INTPWM0 = 0x00000001;   // PWM0 Interrupt Asserted

    constexpr const uint32_t PWM_ISC_INTFAULT3 = 0x00080000; // FAULT3 Interrupt Asserted
    constexpr const uint32_t PWM_ISC_INTFAULT2 = 0x00040000; // FAULT2 Interrupt Asserted
    constexpr const uint32_t PWM_ISC_INTFAULT1 = 0x00020000; // FAULT1 Interrupt Asserted
    constexpr const uint32_t PWM_ISC_INTFAULT0 = 0x00010000; // FAULT0 Interrupt Asserted
    constexpr const uint32_t PWM_ISC_INTPWM3 = 0x00000008;   // PWM3 Interrupt Status
    constexpr const uint32_t PWM_ISC_INTPWM2 = 0x00000004;   // PWM2 Interrupt Status
    constexpr const uint32_t PWM_ISC_INTPWM1 = 0x00000002;   // PWM1 Interrupt Status
    constexpr const uint32_t PWM_ISC_INTPWM0 = 0x00000001;   // PWM0 Interrupt Status

    constexpr const uint32_t PWM_STATUS_FAULT3 = 0x00000008; // Generator 3 Fault Status
    constexpr const uint32_t PWM_STATUS_FAULT2 = 0x00000004; // Generator 2 Fault Status
    constexpr const uint32_t PWM_STATUS_FAULT1 = 0x00000002; // Generator 1 Fault Status
    constexpr const uint32_t PWM_STATUS_FAULT0 = 0x00000001; // Generator 0 Fault Status

    constexpr const uint32_t PWM_FAULTVAL_PWM7 = 0x00000080; // MnPWM7 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM6 = 0x00000040; // MnPWM6 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM5 = 0x00000020; // MnPWM5 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM4 = 0x00000010; // MnPWM4 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM3 = 0x00000008; // MnPWM3 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM2 = 0x00000004; // MnPWM2 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM1 = 0x00000002; // MnPWM1 Fault Value
    constexpr const uint32_t PWM_FAULTVAL_PWM0 = 0x00000001; // MnPWM0 Fault Value

    constexpr const uint32_t PWM_ENUPD_ENUPD7_M = 0x0000C000;     // MnPWM7 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD7_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD7_LSYNC = 0x00008000; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD7_GSYNC = 0x0000C000; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD6_M = 0x00003000;     // MnPWM6 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD6_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD6_LSYNC = 0x00002000; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD6_GSYNC = 0x00003000; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD5_M = 0x00000C00;     // MnPWM5 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD5_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD5_LSYNC = 0x00000800; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD5_GSYNC = 0x00000C00; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD4_M = 0x00000300;     // MnPWM4 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD4_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD4_LSYNC = 0x00000200; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD4_GSYNC = 0x00000300; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD3_M = 0x000000C0;     // MnPWM3 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD3_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD3_LSYNC = 0x00000080; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD3_GSYNC = 0x000000C0; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD2_M = 0x00000030;     // MnPWM2 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD2_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD2_LSYNC = 0x00000020; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD2_GSYNC = 0x00000030; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD1_M = 0x0000000C;     // MnPWM1 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD1_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD1_LSYNC = 0x00000008; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD1_GSYNC = 0x0000000C; // Globally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD0_M = 0x00000003;     // MnPWM0 Enable Update Mode
    constexpr const uint32_t PWM_ENUPD_ENUPD0_IMM = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_ENUPD_ENUPD0_LSYNC = 0x00000002; // Locally Synchronized
    constexpr const uint32_t PWM_ENUPD_ENUPD0_GSYNC = 0x00000003; // Globally Synchronized

    constexpr const uint32_t PWM_CHANNEL_CTL_LATCH = 0x00040000;        // Latch Fault Input
    constexpr const uint32_t PWM_CHANNEL_CTL_MINFLTPER = 0x00020000;    // Minimum Fault Period
    constexpr const uint32_t PWM_CHANNEL_CTL_FLTSRC = 0x00010000;       // Fault Condition Source
    constexpr const uint32_t PWM_CHANNEL_CTL_DBFALLUPD_M = 0x0000C000;  // PWMnDBFALL Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_DBFALLUPD_I = 0x00000000;  // Immediate
    constexpr const uint32_t PWM_CHANNEL_CTL_DBFALLUPD_LS = 0x00008000; // Locally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_DBFALLUPD_GS = 0x0000C000; // Globally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_DBRISEUPD_M = 0x00003000;  // PWMnDBRISE Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_DBRISEUPD_I = 0x00000000;  // Immediate
    constexpr const uint32_t PWM_CHANNEL_CTL_DBRISEUPD_LS = 0x00002000; // Locally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_DBRISEUPD_GS = 0x00003000; // Globally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_DBCTLUPD_M = 0x00000C00;   // PWMnDBCTL Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_DBCTLUPD_I = 0x00000000;   // Immediate
    constexpr const uint32_t PWM_CHANNEL_CTL_DBCTLUPD_LS = 0x00000800;  // Locally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_DBCTLUPD_GS = 0x00000C00;  // Globally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_GENBUPD_M = 0x00000300;    // PWMnGENB Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_GENBUPD_I = 0x00000000;    // Immediate
    constexpr const uint32_t PWM_CHANNEL_CTL_GENBUPD_LS = 0x00000200;   // Locally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_GENBUPD_GS = 0x00000300;   // Globally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_GENAUPD_M = 0x000000C0;    // PWMnGENA Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_GENAUPD_I = 0x00000000;    // Immediate
    constexpr const uint32_t PWM_CHANNEL_CTL_GENAUPD_LS = 0x00000080;   // Locally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_GENAUPD_GS = 0x000000C0;   // Globally Synchronized
    constexpr const uint32_t PWM_CHANNEL_CTL_CMPBUPD = 0x00000020;      // Comparator B Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_CMPAUPD = 0x00000010;      // Comparator A Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_LOADUPD = 0x00000008;      // Load Register Update Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_DEBUG = 0x00000004;        // Debug Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_MODE = 0x00000002;         // Counter Mode
    constexpr const uint32_t PWM_CHANNEL_CTL_ENABLE = 0x00000001;       // PWM Block Enable

    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPBD = 0x00002000;    // Trigger for Counter=PWMnCMPB Down
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPBU = 0x00001000;    // Trigger for Counter=PWMnCMPB Up
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPAD = 0x00000800;    // Trigger for Counter=PWMnCMPA Down
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPAU = 0x00000400;    // Trigger for Counter=PWMnCMPA Up
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCNTLOAD = 0x00000200;  // Trigger for Counter=PWMnLOAD
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCNTZERO = 0x00000100;  // Trigger for Counter=0
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPBD = 0x00000020;   // Interrupt for Counter=PWMnCMPB Down
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPBU = 0x00000010;   // Interrupt for Counter=PWMnCMPB Up
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPAD = 0x00000008;   // Interrupt for Counter=PWMnCMPA Down
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPAU = 0x00000004;   // Interrupt for Counter=PWMnCMPA Up
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCNTLOAD = 0x00000002; // Interrupt for Counter=PWMnLOAD
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCNTZERO = 0x00000001; // Interrupt for Counter=0

    constexpr const uint32_t PWM_CHANNEL_RIS_INTCMPBD = 0x00000020;   // Comparator B Down Interrupt Status
    constexpr const uint32_t PWM_CHANNEL_RIS_INTCMPBU = 0x00000010;   // Comparator B Up Interrupt Status
    constexpr const uint32_t PWM_CHANNEL_RIS_INTCMPAD = 0x00000008;   // Comparator A Down Interrupt Status
    constexpr const uint32_t PWM_CHANNEL_RIS_INTCMPAU = 0x00000004;   // Comparator A Up Interrupt Status
    constexpr const uint32_t PWM_CHANNEL_RIS_INTCNTLOAD = 0x00000002; // Counter=Load Interrupt Status
    constexpr const uint32_t PWM_CHANNEL_RIS_INTCNTZERO = 0x00000001; // Counter=0 Interrupt Status

    constexpr const uint32_t PWM_CHANNEL_ISC_INTCMPBD = 0x00000020;   // Comparator B Down Interrupt
    constexpr const uint32_t PWM_CHANNEL_ISC_INTCMPBU = 0x00000010;   // Comparator B Up Interrupt
    constexpr const uint32_t PWM_CHANNEL_ISC_INTCMPAD = 0x00000008;   // Comparator A Down Interrupt
    constexpr const uint32_t PWM_CHANNEL_ISC_INTCMPAU = 0x00000004;   // Comparator A Up Interrupt
    constexpr const uint32_t PWM_CHANNEL_ISC_INTCNTLOAD = 0x00000002; // Counter=Load Interrupt
    constexpr const uint32_t PWM_CHANNEL_ISC_INTCNTZERO = 0x00000001; // Counter=0 Interrupt

    constexpr const uint32_t PWM_CHANNEL_LOAD_M = 0x0000FFFF; // Counter Load Value
    constexpr const uint32_t PWM_CHANNEL_LOAD_S = 0;

    constexpr const uint32_t PWM_CHANNEL_COUNT_M = 0x0000FFFF; // Counter Value
    constexpr const uint32_t PWM_CHANNEL_COUNT_S = 0;

    constexpr const uint32_t PWM_CHANNEL_CMPA_M = 0x0000FFFF; // Comparator A Value
    constexpr const uint32_t PWM_CHANNEL_CMPA_S = 0;

    constexpr const uint32_t PWM_CHANNEL_CMPB_M = 0x0000FFFF; // Comparator B Value
    constexpr const uint32_t PWM_CHANNEL_CMPB_S = 0;

    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBD_M = 0x00000C00;    // Action for Comparator B Down
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBD_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBD_INV = 0x00000400;  // Invert pwmA
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBD_ZERO = 0x00000800; // Drive pwmA Low
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBD_ONE = 0x00000C00;  // Drive pwmA High
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBU_M = 0x00000300;    // Action for Comparator B Up
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBU_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBU_INV = 0x00000100;  // Invert pwmA
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBU_ZERO = 0x00000200; // Drive pwmA Low
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPBU_ONE = 0x00000300;  // Drive pwmA High
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAD_M = 0x000000C0;    // Action for Comparator A Down
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAD_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAD_INV = 0x00000040;  // Invert pwmA
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAD_ZERO = 0x00000080; // Drive pwmA Low
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAD_ONE = 0x000000C0;  // Drive pwmA High
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAU_M = 0x00000030;    // Action for Comparator A Up
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAU_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAU_INV = 0x00000010;  // Invert pwmA
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAU_ZERO = 0x00000020; // Drive pwmA Low
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAU_ONE = 0x00000030;  // Drive pwmA High
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_M = 0x0000000C;     // Action for Counter=LOAD
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_NONE = 0x00000000;  // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_INV = 0x00000004;   // Invert pwmA
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_ZERO = 0x00000008;  // Drive pwmA Low
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_ONE = 0x0000000C;   // Drive pwmA High
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTZERO_M = 0x00000003;     // Action for Counter=0
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTZERO_NONE = 0x00000000;  // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTZERO_INV = 0x00000001;   // Invert pwmA
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTZERO_ZERO = 0x00000002;  // Drive pwmA Low
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTZERO_ONE = 0x00000003;   // Drive pwmA High

    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_M = 0x00000C00;    // Action for Comparator B Down
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_INV = 0x00000400;  // Invert pwmB
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_ZERO = 0x00000800; // Drive pwmB Low
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_ONE = 0x00000C00;  // Drive pwmB High
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_M = 0x00000300;    // Action for Comparator B Up
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_INV = 0x00000100;  // Invert pwmB
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_ZERO = 0x00000200; // Drive pwmB Low
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_ONE = 0x00000300;  // Drive pwmB High
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAD_M = 0x000000C0;    // Action for Comparator A Down
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAD_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAD_INV = 0x00000040;  // Invert pwmB
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAD_ZERO = 0x00000080; // Drive pwmB Low
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAD_ONE = 0x000000C0;  // Drive pwmB High
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAU_M = 0x00000030;    // Action for Comparator A Up
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAU_NONE = 0x00000000; // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAU_INV = 0x00000010;  // Invert pwmB
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAU_ZERO = 0x00000020; // Drive pwmB Low
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPAU_ONE = 0x00000030;  // Drive pwmB High
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_M = 0x0000000C;     // Action for Counter=LOAD
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_NONE = 0x00000000;  // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_INV = 0x00000004;   // Invert pwmB
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_ZERO = 0x00000008;  // Drive pwmB Low
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_ONE = 0x0000000C;   // Drive pwmB High
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTZERO_M = 0x00000003;     // Action for Counter=0
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTZERO_NONE = 0x00000000;  // Do nothing
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTZERO_INV = 0x00000001;   // Invert pwmB
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTZERO_ZERO = 0x00000002;  // Drive pwmB Low
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTZERO_ONE = 0x00000003;   // Drive pwmB High

    constexpr const uint32_t PWM_CHANNEL_DBCTL_ENABLE = 0x00000001; // Dead-Band Generator Enable

    constexpr const uint32_t PWM_CHANNEL_DBRISE_DELAY_M = 0x00000FFF; // Dead-Band Rise Delay
    constexpr const uint32_t PWM_CHANNEL_DBRISE_DELAY_S = 0;

    constexpr const uint32_t PWM_CHANNEL_DBFALL_DELAY_M = 0x00000FFF; // Dead-Band Fall Delay
    constexpr const uint32_t PWM_CHANNEL_DBFALL_DELAY_S = 0;

    constexpr const uint32_t PWM_CHANNEL_FLTSRC0_FAULT3 = 0x00000008; // Fault3 Input
    constexpr const uint32_t PWM_CHANNEL_FLTSRC0_FAULT2 = 0x00000004; // Fault2 Input
    constexpr const uint32_t PWM_CHANNEL_FLTSRC0_FAULT1 = 0x00000002; // Fault1 Input
    constexpr const uint32_t PWM_CHANNEL_FLTSRC0_FAULT0 = 0x00000001; // Fault0 Input

    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP7 = 0x00000080; // Digital Comparator 7
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP6 = 0x00000040; // Digital Comparator 6
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP5 = 0x00000020; // Digital Comparator 5
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP4 = 0x00000010; // Digital Comparator 4
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP3 = 0x00000008; // Digital Comparator 3
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP2 = 0x00000004; // Digital Comparator 2
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP1 = 0x00000002; // Digital Comparator 1
    constexpr const uint32_t PWM_CHANNEL_FLTSRC1_DCMP0 = 0x00000001; // Digital Comparator 0

    constexpr const uint32_t PWM_CHANNEL_MINFLTPER_M = 0x0000FFFF; // Minimum Fault Period
    constexpr const uint32_t PWM_CHANNEL_MINFLTPER_S = 0;

    constexpr const uint32_t SYSCTL_RCC_USEPWMDIV = 0x00100000; // Enable PWM Clock Divisor
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_M = 0x000E0000;  // PWM Unit Clock Divisor
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_2 = 0x00000000;  // PWM clock /2
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_4 = 0x00020000;  // PWM clock /4
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_8 = 0x00040000;  // PWM clock /8
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_16 = 0x00060000; // PWM clock /16
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_32 = 0x00080000; // PWM clock /32
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_64 = 0x000A0000; // PWM clock /64

    constexpr const uint32_t PWM_CC_USEPWMDIV = 0x00000100; // Use PWM Clock Divisor
    constexpr const uint32_t PWM_CC_PWMDIV_M = 0x00000007;  // PWM Clock Divider
    constexpr const uint32_t PWM_CC_PWMDIV_2 = 0x00000000;  // /2
    constexpr const uint32_t PWM_CC_PWMDIV_4 = 0x00000001;  // /4
    constexpr const uint32_t PWM_CC_PWMDIV_8 = 0x00000002;  // /8
    constexpr const uint32_t PWM_CC_PWMDIV_16 = 0x00000003; // /16
    constexpr const uint32_t PWM_CC_PWMDIV_32 = 0x00000004; // /32
    constexpr const uint32_t PWM_CC_PWMDIV_64 = 0x00000005; // /64

    constexpr const uint32_t SYSCTL_DC1_PWM1 = 0x00200000; // PWM Module 1 Present
    constexpr const uint32_t SYSCTL_DC1_PWM0 = 0x00100000; // PWM Module 0 Present

    constexpr const std::array<uint32_t, 6> triggerType = { {
        PWM_CHANNEL_INTEN_TRCNTZERO,
        PWM_CHANNEL_INTEN_TRCNTLOAD,
        PWM_CHANNEL_INTEN_TRCMPAU,
        PWM_CHANNEL_INTEN_TRCMPAD,
        PWM_CHANNEL_INTEN_TRCMPBU,
        PWM_CHANNEL_INTEN_TRCMPBD,
    } };

    constexpr const std::array<std::pair<hal::tiva::PinConfigPeripheral, hal::tiva::PinConfigPeripheral>, 4> pinConfigPeripheral = { {
        { hal::tiva::PinConfigPeripheral::pwmChannel0, hal::tiva::PinConfigPeripheral::pwmChannel1 },
        { hal::tiva::PinConfigPeripheral::pwmChannel2, hal::tiva::PinConfigPeripheral::pwmChannel3 },
        { hal::tiva::PinConfigPeripheral::pwmChannel4, hal::tiva::PinConfigPeripheral::pwmChannel5 },
        { hal::tiva::PinConfigPeripheral::pwmChannel6, hal::tiva::PinConfigPeripheral::pwmChannel7 },
    } };

    constexpr const std::array<uint32_t, 7> clockDivisor = { {
#if defined(TM4C123)
        0, // DIV_1
        SYSCTL_RCC_PWMDIV_2 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_4 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_8 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_16 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_32 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_64 | SYSCTL_RCC_USEPWMDIV,
#else
        0, // DIV_1
        PWM_CC_PWMDIV_2 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_4 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_8 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_16 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_32 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_64 | PWM_CC_USEPWMDIV,
#endif
    } };

    void SetClockDivisor(PWM0_Type* const pwmBase, hal::tiva::SynchronousPwm::Config::ClockDivisor divisor)
    {
#if defined(TM4C123)
        really_assert(SYSCTL->DC1 & (SYSCTL_DC1_PWM0 | SYSCTL_DC1_PWM1));
        SYSCTL->RCC = ((SYSCTL->RCC & ~(SYSCTL_RCC_USEPWMDIV | SYSCTL_RCC_PWMDIV_M)) | clockDivisor[static_cast<std::size_t>(divisor)]);
#else
        pwmBase->CC = ((pwmBase->CC & ~(PWM_CC_USEPWMDIV | PWM_CC_PWMDIV_M)) | clockDivisor[static_cast<std::size_t>(divisor)]);
#endif
    }

    uint32_t GetClockDivisor(PWM0_Type* const pwmBase)
    {
#if defined(TM4C123)
        volatile auto result = (SYSCTL->RCC & SYSCTL_RCC_PWMDIV_M) >> 17;

        if (!(SYSCTL->RCC & SYSCTL_RCC_USEPWMDIV))
            return 1;
        else
            return 1U << ((result >> 1) + 1);
#else
        auto result = pwmBase->CC & PWM_CC_PWMDIV_M;

        if (!(pwmBase->CC & PWM_CC_USEPWMDIV))
            return 1;
        else
            return 1U << (result + 1);
#endif
    }

    float GetSystemCoreClock()
    {
        return static_cast<float>(SystemCoreClock);
    }

    uint32_t ToPeriod(PWM0_Type* const pwmBase, hal::Hertz& baseFrequency)
    {
        auto pwmClock = SystemCoreClock / GetClockDivisor(pwmBase);

        return pwmClock / baseFrequency.Value();
    }

    bool IsCenterAligned(hal::tiva::SynchronousPwm::Config::Control::Mode mode)
    {
        return mode == hal::tiva::SynchronousPwm::Config::Control::Mode::centerAligned;
    }

#if defined(TM4C129)
    constexpr std::size_t numberOfPwms = 1;
#else
    constexpr std::size_t numberOfPwms = 2;
#endif

    constexpr std::array<uint32_t, numberOfPwms> peripheralPwmArray = { {
        PWM0_BASE,
#if defined(TM4C123)
        PWM1_BASE,
#endif
    } };

    const infra::MemoryRange<PWM0_Type* const> peripheralPwm = infra::ReinterpretCastMemoryRange<PWM0_Type* const>(infra::MakeRange(peripheralPwmArray));
}

namespace hal::tiva
{
    uint32_t SynchronousPwm::Config::Control::Value() const
    {
        auto value = static_cast<uint32_t>(mode) << 1;

        value |= static_cast<uint32_t>(debugMode) << 2;

        value |= static_cast<uint32_t>(updateMode == UpdateMode::globally) << 3;
        value |= static_cast<uint32_t>(updateMode == UpdateMode::globally) << 4;
        value |= static_cast<uint32_t>(updateMode == UpdateMode::globally) << 5;

        value |= static_cast<uint32_t>(updateMode) << 6;
        value |= static_cast<uint32_t>(updateMode) << 8;
        value |= static_cast<uint32_t>(updateMode) << 10;
        value |= static_cast<uint32_t>(updateMode) << 12;
        value |= static_cast<uint32_t>(updateMode) << 14;

        return value & 0x7fffe;
    }

    SynchronousPwm::Generator::Generator(PinChannel& pins, uint32_t pwmOffset, GeneratorIndex generator)
        : address(PwmChannel(pwmOffset, generator))
    {
        auto index = static_cast<uint8_t>(generator) * 2;
        auto pinConfig = pinConfigPeripheral.at(infra::enum_cast(generator));

        if (pins.usesChannelA)
        {
            a.Emplace(pins.pinA, pinConfig.first);
            enable |= 1 << index;
        }

        if (pins.usesChannelB)
        {
            b.Emplace(pins.pinB, pinConfig.second);
            enable |= 1 << (index + 1);
        }
    }

    SynchronousPwm::SynchronousPwm(uint8_t aPwmIndex, infra::MemoryRange<PinChannel> channels, const Config& config)
        : pwmIndex(aPwmIndex)
        , config(config)
    {
        really_assert(!channels.empty() && channels.size() <= generators.max_size());

        for (auto& channel : channels)
            generators.emplace_back(channel, peripheralPwmArray[pwmIndex], channel.generator);

        Initialize();
    }

    SynchronousPwm::~SynchronousPwm()
    {
        auto zero = hal::Percent(0);

        for (auto& generator : generators)
        {
            SetComparator(generator, zero);
            Sync();
            peripheralPwm[pwmIndex]->ENABLE &= ~generator.enable;
            generator.address->CTL &= ~PWM_CHANNEL_CTL_ENABLE;
        }

        DisableClock();
    }

    void SynchronousPwm::Initialize()
    {
        EnableClock();
        SetClockDivisor(peripheralPwm[pwmIndex], config.clockDivisor);

        for (auto& generator : generators)
            GeneratorConfiguration(generator);

        if (config.trigger)
            generators[0].address->INTEN |= triggerType[static_cast<uint32_t>(*config.trigger)];
    }

    void SynchronousPwm::SetBaseFrequency(hal::Hertz baseFrequency)
    {
        auto load = ToPeriod(peripheralPwm[pwmIndex], baseFrequency);
        load = IsCenterAligned(config.control.mode) ? load / 2 : load - 1;
        really_assert(load < 0xffff);

        for (auto& generator : generators)
        {
            if (generator.a)
                generator.address->LOAD = load;
            if (generator.b)
                generator.address->LOAD = load;
        }

        Sync();
    }

    void SynchronousPwm::Start(hal::Percent dutyCycle)
    {
        really_assert(generators.size() >= 1);

        for (auto& generator : generators)
            SetComparator(generator, dutyCycle);

        Sync();
    }

    void SynchronousPwm::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2)
    {
        really_assert(generators.size() == 2);

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);

        Sync();
    }

    void SynchronousPwm::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3)
    {
        really_assert(generators.size() == 3);

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);
        SetComparator(generators[2], dutyCycle3);

        Sync();
    }

    void SynchronousPwm::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3, hal::Percent dutyCycle4)
    {
        really_assert(generators.size() == 4);

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);
        SetComparator(generators[2], dutyCycle3);
        SetComparator(generators[3], dutyCycle4);

        Sync();
    }

    void SynchronousPwm::Stop()
    {
        auto zero = hal::Percent(0);

        for (auto& generator : generators)
        {
            SetComparator(generator, zero);
            Sync();
        }
    }

    void SynchronousPwm::GeneratorConfiguration(Generator& generator)
    {
        if (generator.a || generator.b)
        {
            generator.address->CTL |= config.control.Value();
            generator.address->GENA = IsCenterAligned(config.control.mode) ? (PWM_CHANNEL_GENA_ACTCMPAU_ONE | PWM_CHANNEL_GENA_ACTCMPAD_ZERO) : (PWM_CHANNEL_GENA_ACTLOAD_ONE | PWM_CHANNEL_GENA_ACTCMPAD_ZERO);
            generator.address->GENB = IsCenterAligned(config.control.mode) ? (PWM_CHANNEL_GENB_ACTCMPBU_ONE | PWM_CHANNEL_GENB_ACTCMPBD_ZERO) : (PWM_CHANNEL_GENB_ACTLOAD_ONE | PWM_CHANNEL_GENB_ACTCMPBD_ZERO);

            if (config.deadTime)
            {
                generator.address->DBCTL |= PWM_CHANNEL_DBCTL_ENABLE;
                generator.address->DBFALL = config.deadTime->fallInClockCycles;
                generator.address->DBRISE = config.deadTime->riseInClockCycles;
            }
            else
                generator.address->DBCTL &= ~PWM_CHANNEL_DBCTL_ENABLE;

            peripheralPwm[pwmIndex]->ENABLE |= generator.enable;
        }
    }

    void SynchronousPwm::SetComparator(Generator& generator, hal::Percent& dutyCycle)
    {
        really_assert(dutyCycle.Value() <= 100);

        auto comparator = GetLoad(generator) * dutyCycle.Value() / 100;

        if (IsCenterAligned(config.control.mode))
            comparator = comparator / 2;

        if (generator.a)
            generator.address->CMPA = generator.address->LOAD - comparator;
        if (generator.b)
            generator.address->CMPB = generator.address->LOAD - comparator;
    }

    void SynchronousPwm::Sync()
    {
        peripheralPwm[pwmIndex]->SYNC = 0x0000000F;
    }

    uint32_t SynchronousPwm::GetLoad(Generator& generator)
    {
        if (IsCenterAligned(config.control.mode))
            return generator.address->LOAD * 2;
        else
            return generator.address->LOAD - 1;
    }

    void SynchronousPwm::EnableClock()
    {
        SYSCTL->RCGCPWM |= (1 << pwmIndex);

        while (!(SYSCTL->PRPWM & (1 << pwmIndex)))
        {
        }
    }

    void SynchronousPwm::DisableClock()
    {
        SYSCTL->RCGCPWM &= ~(1 << pwmIndex);
    }

    uint16_t SynchronousPwm::CalculateDeadTimeCycles(std::chrono::nanoseconds deadTime, Config::ClockDivisor divisor)
    {
        static constexpr std::array<uint32_t, 7> divisorValues = { { 1, 2, 4, 8, 16, 32, 64 } };

        auto divisorValue = divisorValues[static_cast<uint32_t>(divisor)];
        auto pwmClockFreq = GetSystemCoreClock() / static_cast<float>(divisorValue);
        auto deadTimeNs = static_cast<float>(deadTime.count());
        auto cycles = static_cast<uint32_t>(deadTimeNs * pwmClockFreq / 1e9);

        really_assert(cycles <= 4095);

        return static_cast<uint16_t>(cycles);
    }
}
