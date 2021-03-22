#ifndef EVMU_CPU__H
#define EVMU_CPU__H


#include <evmu/hw/evmu_cpu.h>
#include "evmu_peripheral_.h"

#ifdef __cplusplus
extern "C" {
#endif

struct EvmuMemory_;

typedef enum EVMU_STACK_FRAME_TYPE {
    EVMU_STACK_FRAME_UNKNOWN,
    EVMU_STACK_FRAME_FUNCTION,
    EVMU_STACK_FRAME_FIRMWARE,
    EVMU_STACK_FRAME_SUBROUTINE,
    EVMU_STACK_FRAME_INTERRUPT,
    EVMU_STACK_FRAME_TYPE_COUNT
} EVMU_STACK_FRAME_TYPE;

typedef struct EvmuStackFrame_ {
    EvmuAddress             pcReturn; //callee for callR, callF, hardcoded for interrupts
    EvmuAddress             pcStart;
    EvmuAddress             pc;

    uint8_t                 stackStart;
    EVMU_STACK_FRAME_TYPE   frameType;
    EvmuBool                systemMode;
} EvmuStackFrame_;


typedef struct EvmuCpu_ {
    EvmuPeripheral_                     peripheral;
    struct EvmuMemory_*                 pMemory;

    EvmuAddress                         pc;
    struct {
        EvmuInstruction                 encoded;
        const EvmuInstructionFormat*    pFormat;
        EvmuDecodedOperands             operands;
        uint8_t                         opcode;
        uint8_t                         elapsedCycles;
    } curInstr;
} EvmuCpu_;


//}

EVMU_API evmuCpuDriverReset_(EvmuCpu_* pCpu) {
    memset(pCpu, 0, sizeof(EvmuCpu_));
}

EVMU_API evmuCpuDriverStateSave_(EvmuCpu_* pCpu, EvmuFile hFile) {
    EVMU_API_FILE_WRITE(hFile, &pCpu->pc, sizeof(pCpu->pc));
    EVMU_API_FILE_WRITE(hFile, &pCpu->curInstr, sizeof(pCpu->curInstr));
}

EVMU_API evmuCpuDriverStateLoad_(EvmuCpu_* pCpu, EvmuFile hFile) {
    EVMU_API_FILE_READ(hFile, &pCpu->pc, sizeof(pCpu->pc));
    EVMU_API_FILE_READ(hFile, &pCpu->curInstr, sizeof(pCpu->curInstr));
    //adjust base peripheral pointers and driver and shit!!!

}

EVMU_API evmuCpuDriverProperty_(EvmuCpu_* pCpu,  EvmuEnum property, void* pData, EvmuSize* pSize) {

    switch(property) {
    case EVMU_CPU_PROPERTY_PROGRAM_COUNTER:
        memcpy(pData, &pCpu->pc, sizeof(pCpu->pc));
        *pSize = sizeof(pCpu->pc);
        break;
    case EVMU_CPU_PROPERTY_CLOCK_SOURCE:

    break;

    default:
break;
    }
}

EVMU_API evmuCpuDriverPropertySet_(EvmuCpu_* pCpu,  EvmuEnum property, const void* pData, EvmuSize* pSize) {

    switch(property) {
    case EVMU_CPU_PROPERTY_PROGRAM_COUNTER:
        memcpy(&pCpu->pc, pData, sizeof(pCpu->pc));
        *pSize = sizeof(pCpu->pc);
        break;
    case EVMU_CPU_PROPERTY_CLOCK_SOURCE:

    break;

    default:
break;
    }
}

EVMU_API evmuCpuDriverUpdate_(EvmuCpu_* pCpu, EvmuTicks ticks);
EVMU_API evmuCpuDriverInit_(EvmuCpu_* pCpu);





EvmuPeripheralInitFn            pFnInit;
EvmuPeripheralDeinitFn          pFnDeinit;
EvmuPeripheralResetFn           pFnReset;
EvmuPeripheralUpdateFn          pFnUpdate;
EvmuPeripheralEventFn           pFnEvent;
EvmuPeripheralMemoryReadFn      pFnMemoryRead;
EvmuPeripheralMemoryWriteFn     pFnMemoryWrite;
EvmuPeripheralPropertyFn        pFnProperty;
EvmuPeripheralPropertySetFn     pFnPropertySet;
EvmuPeripheralStateSaveFn       pFnStateSave;
EvmuPeripheralStateLoadFn       pFnStateLoad;
EvmuPeripheralParseCmdLineArgs  pFnParseCmdLineArgs;
EvmuPeripheralDebugCommandFn    pFnDebugCmd;
//const EvmuPeripheralDriver* evmuCpuDriver_(void) {
    static const EvmuPeripheralDriver evmuCpuDriver_ = {
        EVMU_PERIPHERAL_CPU,
        "Cpu",
        "Sanyo LC86k 8-bit CPU",
        sizeof(EvmuCpu_),
        {
            .pFnInit        = (EvmuPeripheralInitFn)        &evmuCpuDriverInit_,
            .pFnUpdate      = (EvmuPeripheralUpdateFn)      &evmuCpuDriverUpdate_,
            .pFnReset       = (EvmuPeripheralResetFn)       &evmuCpuDriverReset_,
            .pFnProperty    = (EvmuPeripheralPropertyFn)    &evmuCpuDriverProperty_,
            .pFnPropertySet = (EvmuPeripheralPropertySetFn) &evmuCpuDriverPropertySet_,
            .pFnStateSave   = (EvmuPeripheralStateSaveFn)   &evmuCpuDriverStateSave_,
            .pFnStateLoad   = (EvmuPeripheralStateLoadFn)   &evmuCpuDriverStateLoad_
        },
        .ppProperties = ((const EvmuPeripheralProperty*[]){
            (&(const EvmuPeripheralProperty){
                EVMU_CPU_PROPERTY_PROGRAM_COUNTER,
                "Program Counter",
                "Current Instruction Pointer Address",
                GBL_VARIANT_TYPE_INT,
                sizeof(int)
            }),
            (&(const EvmuPeripheralProperty){
                EVMU_CPU_PROPERTY_CLOCK_SOURCE,
                "Clock Source",
                "Oscillator Source driving CPU execution",
                GBL_VARIANT_TYPE_INT,
                sizeof(int)
            })
        })
    };
#ifdef __cplusplus
}
#endif

#endif // EVMU_CPU__H
