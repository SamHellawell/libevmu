/*! \file
 *  \brief EvmuEmulator top-level emulation module
 *
 *  This file provides everything pertaining to the public
 *  API of the EvmuEmulator module.
 *
 *  \bug Pac-It runs slowly as hell
 *  \bug Mini Pac-Man partial audio
 *  \bug Sleep mode not handled properly
 *  \bug Scene doesn't immediately update upon resizing
 *  \bug Scene doesn't immediately update upon modifying effects
 *
 *  \todo manage EvmuDevice children lifetimes
 *  \todo handle command-line options
 *  \todo persist user-settings
 *
 *  \author    2023 Falco Girgis
 *  \copyright MIT License
*/
#ifndef EVMU_EMULATOR_H
#define EVMU_EMULATOR_H

#include <gimbal/core/gimbal_module.h>
#include "evmu_ibehavior.h"

/*! \name  Type System
 *  \brief Type UUID and cast operators
 *  @{
 */
#define EVMU_EMULATOR_TYPE                  (GBL_TYPEOF(EvmuEmulator))                       //!< Type UUID for EvmuEmulator
#define EVMU_EMULATOR(instance)             (GBL_INSTANCE_CAST(instance,  EvmuEmulator))     //!< Function-style GblInstance cast
#define EVMU_EMULATOR_CLASS(klass)          (GBL_CLASS_CAST(klass, EvmuEmulator))            //!< Function-style GblClass cast
#define EVMU_EMULATOR_GET_CLASS(instance)   (GBL_INSTANCE_GET_CLASS(instance, EvmuEmulator)) //!< Get EvmuEmulatorClass from GblInstance
//! @}

#define GBL_SELF_TYPE   EvmuEmulator

GBL_DECLS_BEGIN

GBL_FORWARD_DECLARE_STRUCT(EvmuDevice);

//! Function signature used for iterator callback with EvmuEmulator_foreachDevice()
typedef GblBool (*EvmuEmulatorIterFn)(GBL_CSELF, EvmuDevice* pDevice, void* pClosure);

/*! \struct     EvmuEmulatorClass
 *  \extends    GblModuleClass
 *  \implements EvmuIBehaviorClass
 *  \brief      GblClass structure for EvmuEmulator
 *
 *  No public methods
 *
 *  \sa EvmuEmulator
 */
GBL_CLASS_DERIVE_EMPTY(EvmuEmulator, GblModule, EvmuIBehavior)
\
/*! \struct     EvmuEmulator
 *  \extends    GblModule
 *  \implements EvmuIBehavior
 *  \brief      Top-level module for emulator
 *
 *  EvmuEmulator is a top-level module object for the
 *  emulation core.
 *
 *  \sa EvmuEmulatorClass
 */
GBL_INSTANCE_DERIVE_EMPTY(EvmuEmulator, GblModule)

EVMU_EXPORT GblType       EvmuEmulator_type          (void)                          GBL_NOEXCEPT;
EVMU_EXPORT GblVersion    EvmuEmulator_version       (void)                          GBL_NOEXCEPT;

EVMU_EXPORT EvmuEmulator* EvmuEmulator_create        (void)                          GBL_NOEXCEPT;
EVMU_EXPORT GblRefCount   EvmuEmulator_unref         (GBL_SELF)                      GBL_NOEXCEPT;

EVMU_EXPORT EVMU_RESULT   EvmuEmulator_addDevice     (GBL_SELF, EvmuDevice* pDevice) GBL_NOEXCEPT;
EVMU_EXPORT EVMU_RESULT   EvmuEmulator_removeDevice  (GBL_SELF, EvmuDevice* pDevice) GBL_NOEXCEPT;
EVMU_EXPORT size_t        EvmuEmulator_deviceCount   (GBL_CSELF)                     GBL_NOEXCEPT;
EVMU_EXPORT EvmuDevice*   EvmuEmulator_device        (GBL_CSELF, size_t index)       GBL_NOEXCEPT;
EVMU_EXPORT GblBool       EvmuEmulator_foreachDevice (GBL_CSELF,
                                                      EvmuEmulatorIterFn pFnIt,
                                                      void*              pClosure)   GBL_NOEXCEPT;

GBL_DECLS_END

#undef GBL_SELF_TYPE

#endif // EVMU_EMULATOR_H
