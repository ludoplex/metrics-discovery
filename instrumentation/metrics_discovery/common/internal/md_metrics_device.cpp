/*========================== begin_copyright_notice ============================

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

//     File Name:  md_metrics_device.cpp

//     Abstract:   C++ Metrics Discovery internal metrics device implementation

#include "md_metrics_device.h"
#include "md_adapter.h"
#include "md_concurrent_group.h"
#include "md_oa_concurrent_group.h"
#include "md_oam_concurrent_group.h"
#include "md_equation.h"
#include "md_information.h"
#include "md_metric.h"
#include "md_metric_set.h"
#include "md_override.h"

#include "md_driver_ifc.h"

#include <cstring>
#include <cmath>

namespace MetricsDiscoveryInternal
{

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     CMetricsDevice constructor
    //
    // Description:
    //     Constructor. Sends DeviceInfoParam escape.
    //
    //////////////////////////////////////////////////////////////////////////////
    CMetricsDevice::CMetricsDevice( CAdapter& adapter, CDriverInterface& driverInterface, const uint32_t subDeviceIndex /* = 0 */ )
        : m_params{}
        , m_groupsVector()
        , m_overridesVector()
        , m_adapter( adapter )
        , m_driverInterface( driverInterface )
        , m_symbolSet( *this, driverInterface )
        , m_streamId( -1 )
        , m_streamConfigId( -1 )
        , m_subDeviceIndex( subDeviceIndex )
        , m_platformIndex( 0 )
        , m_gtType( GT_TYPE_UNKNOWN )
        , m_isOpenedFromFile( false )
        , m_referenceCounter( 0 )
        , m_oaBuferCount( 0 )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        m_params.DeltaFunctionsCount       = DELTA_FUNCTION_LAST_1_0;
        m_params.EquationOperationsCount   = EQUATION_OPER_LAST_1_0;
        m_params.EquationElementTypesCount = EQUATION_ELEM_LAST_1_0;

        m_params.Version.MajorNumber = MD_API_MAJOR_NUMBER_CURRENT;
        m_params.Version.MinorNumber = MD_API_MINOR_NUMBER_CURRENT;
        m_params.Version.BuildNumber = MD_API_BUILD_NUMBER_CURRENT;

        m_params.GlobalSymbolsCount    = m_symbolSet.GetSymbolCount();
        m_params.ConcurrentGroupsCount = 0;
        m_params.OverrideCount         = 0;

        m_params.DeviceName = GetCopiedCString( m_adapter.GetParams()->ShortName, adapterId );

        m_groupsVector.reserve( GROUPS_VECTOR_INCREASE );
        m_overridesVector.reserve( OVERRIDES_VECTOR_INCREASE );

        GTDIDeviceInfoParamExtOut out = {};

        if( m_driverInterface.SendDeviceInfoParamEscape( GTDI_DEVICE_PARAM_PLATFORM_INDEX, &out ) == CC_OK )
        {
            m_platformIndex = out.ValueUint32;
            MD_LOG_A( adapterId, LOG_INFO, "Metrics device platform index: %u", m_platformIndex );
        }
        if( m_driverInterface.SendDeviceInfoParamEscape( GTDI_DEVICE_PARAM_GT_TYPE, &out, this ) == CC_OK )
        {
            m_gtType = (TGTType) ( 1 << out.ValueUint32 );
            MD_LOG_A( adapterId, LOG_INFO, "GT_TYPE is %u", m_gtType );
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     CMetricsDevice destructor
    //
    // Description:
    //     Deallocates memory.
    //
    //////////////////////////////////////////////////////////////////////////////
    CMetricsDevice::~CMetricsDevice()
    {
        MD_SAFE_DELETE_ARRAY( m_params.DeviceName );

        ClearVector( m_groupsVector );
        ClearVector( m_overridesVector );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetParams
    //
    // Description:
    //     Returns metrics device params.
    //
    // Output:
    //     TMetricsDeviceParams_1_0*  - pointer to metrics device params
    //
    //////////////////////////////////////////////////////////////////////////////
    TMetricsDeviceParams_1_2* CMetricsDevice::GetParams( void )
    {
        return &m_params;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetConcurrentGroup
    //
    // Description:
    //     Returns chosen concurrent group or null if not exists.
    //
    // Input:
    //     uint32_t index          - index of concurrent group
    //
    // Output:
    //     IConcurrentGroupLatest* - pointer to concurrent group
    //
    //////////////////////////////////////////////////////////////////////////////
    IConcurrentGroupLatest* CMetricsDevice::GetConcurrentGroup( uint32_t index )
    {
        if( index < m_groupsVector.size() )
        {
            return m_groupsVector[index];
        }

        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetGlobalSymbol
    //
    // Description:
    //     Returns chosen global symbol or null if not exists.
    //
    // Input:
    //     uint32_t index      - index of global symbol
    //
    // Output:
    //     TGlobalSymbol_1_0*  - pointer to global symbol
    //
    //////////////////////////////////////////////////////////////////////////////
    TGlobalSymbol_1_0* CMetricsDevice::GetGlobalSymbol( uint32_t index )
    {
        return m_symbolSet.GetSymbol( index );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetGlobalSymbolValueByName
    //
    // Description:
    //     Returns chosen global symbol by name or null if not exists.
    //
    // Input:
    //     const char * name   - name of global symbol
    //
    // Output:
    //     TTypedValue_1_0*    - pointer to global symbol
    //
    //////////////////////////////////////////////////////////////////////////////
    TTypedValue_1_0* CMetricsDevice::GetGlobalSymbolValueByName( const char* name )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, name, nullptr );

        return m_symbolSet.GetSymbolValueByName( name );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetLastError
    //
    // Description:
    //     Returns last error from enum TCompletionCode.
    //
    // Output:
    //     TCompletionCode - last error from enum
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::GetLastError()
    {
        return CC_LAST_1_0;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetCpuGpuTimestamps
    //
    // Description:
    //     Retrieves both GPU and CPU timestamp at the same time.
    //
    // Input:
    //     uint64_t* gpuTimestampNs         - (out) GPU timestamp in ns
    //     uint64_t* cpuTimestampNs         - (out) CPU timestamp in ns
    //     uint32_t* cpuId                  - (out) CPU id
    //     uint64_t* correlationIndicatorNs - (out) correlation indicator in ns
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::GetGpuCpuTimestamps( uint64_t* gpuTimestampNs, uint64_t* cpuTimestampNs, uint32_t* cpuId, uint64_t* correlationIndicatorNs )
    {
        if( !gpuTimestampNs && !cpuTimestampNs )
        {
            return CC_ERROR_INVALID_PARAMETER;
        }

        uint64_t        gpuTS = 0, cpuTS = 0, correlationIndicator = 0;
        uint32_t        cpuID = 0;
        TCompletionCode ret   = m_driverInterface.GetGpuCpuTimestamps( *this, &gpuTS, &cpuTS, &cpuID, &correlationIndicator );

        if( ret == CC_OK )
        {
            if( gpuTimestampNs )
            {
                *gpuTimestampNs = gpuTS;
            }
            if( cpuTimestampNs )
            {
                *cpuTimestampNs = cpuTS;
            }
            if( cpuId )
            {
                *cpuId = cpuID;
            }
            if( correlationIndicatorNs )
            {
                *correlationIndicatorNs = correlationIndicator;
            }
        }

        return ret;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetCpuGpuTimestamps
    //
    // Description:
    //     Retrieves both GPU and CPU timestamp at the same time.
    //
    // Input:
    //     uint64_t* gpuTimestampNs - (out) GPU timestamp in ns
    //     uint64_t* cpuTimestampNs - (out) CPU timestamp in ns
    //     uint32_t* cpuId          - (out) CPU id
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::GetGpuCpuTimestamps( uint64_t* gpuTimestampNs, uint64_t* cpuTimestampNs, uint32_t* cpuId )
    {
        return GetGpuCpuTimestamps( gpuTimestampNs, cpuTimestampNs, cpuId, nullptr );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetOverride
    //
    // Description:
    //     Returns the chosen override object or null if index doesn't exist.
    //
    // Input:
    //     uint32_t index - index of an override
    //
    // Output:
    //     IOverride_1_2* - chosen override object or null
    //
    //////////////////////////////////////////////////////////////////////////////
    IOverride_1_2* CMetricsDevice::GetOverride( uint32_t index )
    {
        if( index < m_overridesVector.size() )
        {
            return m_overridesVector[index];
        }

        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetOverrideByName
    //
    // Description:
    //     Returns the chosen override object or nullptr if index doesn't exist.
    //
    // Input:
    //     const char* symbolName - name of an override
    //
    // Output:
    //     IOverride_1_2* - chosen override object or nullptr
    //
    //////////////////////////////////////////////////////////////////////////////
    IOverride_1_2* CMetricsDevice::GetOverrideByName( const char* symbolName )
    {
        MD_CHECK_PTR_RET_A( m_adapter.GetAdapterId(), symbolName, nullptr );

        for( auto& override : m_overridesVector )
        {
            if( override && ( strcmp( symbolName, override->GetParams()->SymbolName ) == 0 ) )
            {
                return override;
            }
        }

        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     AddConcurrentGroup
    //
    // Description:
    //     Adds concurrent group to the metrics device.
    //
    // Input:
    //     const char*       symbolicName        - concurrent group name
    //     const char*       shortName           - concurrent group short name
    //     TByteArrayLatest* platformMask        - platform mask
    //     const uint32_t    measurementTypeMask - measurement type mask
    //
    // Output:
    //     bool&             isSupported         - true if concurrent group is supported on given platform
    //     CConcurrentGroup*                     - pointer to newly created concurrent group
    //
    //////////////////////////////////////////////////////////////////////////////
    CConcurrentGroup* CMetricsDevice::AddConcurrentGroup( const char* symbolicName, const char* shortName, const uint32_t measurementTypeMask, TByteArrayLatest* platformMask, bool& isSupported )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        CConcurrentGroup* group = nullptr;
        isSupported             = true;

        if( !IsPlatformPresentInMask( platformMask, m_platformIndex, adapterId ) )
        {
            isSupported = false;
            return nullptr;
        };

        if( strstr( symbolicName, "OAM" ) != nullptr )
        {
            if( !COAMConcurrentGroup::IsSupported( symbolicName, *this ) )
            {
                isSupported = false;
                return nullptr;
            }

            group = new( std::nothrow ) COAMConcurrentGroup( *this, symbolicName, shortName, measurementTypeMask );
        }
        else if( strstr( symbolicName, "OA" ) != nullptr )
        {
            group = new( std::nothrow ) COAConcurrentGroup( *this, symbolicName, shortName, measurementTypeMask );
        }
        else
        {
            group = new( std::nothrow ) CConcurrentGroup( *this, symbolicName, shortName, measurementTypeMask );
        }

        MD_CHECK_PTR_RET_A( adapterId, group, nullptr );

        m_groupsVector.push_back( group );
        m_params.ConcurrentGroupsCount = m_groupsVector.size();

        return group;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     AddOverride
    //
    // Description:
    //     Adds the given override to MetricsDevice if available on current driver interface and
    //     platform.
    //     All the params are set in specialized constructors.
    //
    // Output:
    //     IOverride_1_2* - added override or null
    //
    //////////////////////////////////////////////////////////////////////////////
    IOverride_1_2* CMetricsDevice::AddOverride( TOverrideType overrideType )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();
        // 1. CHECK AVAILABILITY ON CURRENT DRIVER INTERFACE
        if( !m_driverInterface.IsOverrideAvailable( overrideType ) )
        {
            MD_LOG_A( adapterId, LOG_INFO, "Override %u not available on the current driver interface", overrideType );
            return nullptr;
        }

        // 2. CREATE OVERRIDE OBJECT
        IOverride_1_2* override = nullptr;
        switch( overrideType )
        {
            case OVERRIDE_TYPE_FREQUENCY:
                override = new( std::nothrow ) COverride<OVERRIDE_TYPE_FREQUENCY>( *this );
                break;
            case OVERRIDE_TYPE_NULL_HARDWARE:
                override = new( std::nothrow ) COverride<OVERRIDE_TYPE_NULL_HARDWARE>( *this );
                break;
            case OVERRIDE_TYPE_MULTISAMPLED_QUERY:
                override = new( std::nothrow ) COverride<OVERRIDE_TYPE_MULTISAMPLED_QUERY>( *this );
                break;
            case OVERRIDE_TYPE_EXTENDED_QUERY:
                override = new( std::nothrow ) COverride<OVERRIDE_TYPE_EXTENDED_QUERY>( *this );
                break;
            case OVERRIDE_TYPE_FLUSH_GPU_CACHES:
                override = new( std::nothrow ) COverride<OVERRIDE_TYPE_FLUSH_GPU_CACHES>( *this );
                break;
#if defined( _RELEASE_INTERNAL )
            case OVERRIDE_TYPE_FREQUENCY_CHANGE_REPORTS:
                override = new( std::nothrow ) COverride<OVERRIDE_TYPE_FREQUENCY_CHANGE_REPORTS>( *this );
                break;
#endif
            default:
                break;
        }

        // 3. CHECK AVAILABILITY ON CURRENT PLATFORM
        MD_CHECK_PTR_RET_A( adapterId, override, nullptr );
        if( IsPlatformTypeOf( static_cast<COverrideCommon*>( override )->GetParamsInternal()->PlatformMask ) )
        {
            // Add override and update count
            m_overridesVector.push_back( override );
            m_params.OverrideCount = m_overridesVector.size();
            MD_LOG_A( adapterId, LOG_INFO, "%s - added", override->GetParams()->SymbolName );
        }
        else
        {
            // Override isn't available on the current platform
            MD_LOG_A( adapterId, LOG_INFO, "%s - not available", override->GetParams()->SymbolName );
            MD_SAFE_DELETE( override );
        }

        return override;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     AddOverrides
    //
    // Description:
    //     Fills override vector in MetricsDevice with previously defined overrides.
    //     All the params are set in specialized constructors.
    //
    // Output:
    //     TCompletionCode - result, *CC_OK* means success
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::AddOverrides()
    {
        TCompletionCode ret = CC_OK;

        AddOverride( OVERRIDE_TYPE_FREQUENCY );
        AddOverride( OVERRIDE_TYPE_NULL_HARDWARE );
        AddOverride( OVERRIDE_TYPE_EXTENDED_QUERY );
        AddOverride( OVERRIDE_TYPE_MULTISAMPLED_QUERY );
        AddOverride( OVERRIDE_TYPE_FLUSH_GPU_CACHES );
#if defined( _RELEASE_INTERNAL )
        AddOverride( OVERRIDE_TYPE_FREQUENCY_CHANGE_REPORTS );
#endif
        // ...

        return ret;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     IsPlatformTypeOf
    //
    // Description:
    //     Checks if current platform is given type.
    //
    // Input:
    //     const TByteArrayLatest* platformMask - platform mask byte array
    //     uint32_t                gtMask - gt type mask in TGTType notation
    //
    // Output:
    //     bool                           - result
    //
    //////////////////////////////////////////////////////////////////////////////
    bool CMetricsDevice::IsPlatformTypeOf( TByteArrayLatest* platformMask, uint32_t gtMask /*= GT_TYPE_ALL*/ )
    {
        return ( m_gtType & gtMask ) && IsPlatformPresentInMask( platformMask, m_platformIndex, m_adapter.GetAdapterId() );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     IsPavpDisabled
    //
    // Description:
    //     Checks if the PAVP_DISABLED bit in capabilities global symbol is set.
    //
    // Output:
    //     bool - result
    //
    //////////////////////////////////////////////////////////////////////////////
    bool CMetricsDevice::IsPavpDisabled( uint32_t capabilities )
    {
        return ( capabilities & GTDI_CAPABILITY_PAVP_DISABLED ) > 0;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     SaveToFile
    //
    // Description:
    //     Saves a custom part of MetricsDevice to file.
    //
    // Input:
    //     const char *    fileName        - file name
    //     const uint32_t  minMajorVersion - required major MDAPI version to save to file
    //     const uint32_t  minMinorVersion - required minor MDAPI version to save to file
    //
    // Output:
    //     TCompletionCode             - result
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::SaveToFile( const char* fileName, const uint32_t minMajorApiVersion /* =0*/, const uint32_t minMinorApiVersion /* =0*/ )
    {
        TCompletionCode retVal     = CC_OK;
        FILE*           metricFile = nullptr;
        const uint32_t  adapterId  = m_adapter.GetAdapterId();

        iu_fopen_s( &metricFile, fileName, "wb" );
        MD_CHECK_PTR_RET_A( adapterId, metricFile, CC_ERROR_FILE_NOT_FOUND );

        // Specific key indicating plain text MDAPI file
        fwrite( MD_METRICS_FILE_KEY_3_0, sizeof( char ), sizeof( MD_METRICS_FILE_KEY_3_0 ), metricFile );

        // Minimal api vesrion
        fwrite( &minMajorApiVersion, sizeof( uint32_t ), 1, metricFile );
        fwrite( &minMinorApiVersion, sizeof( uint32_t ), 1, metricFile );

        // m_platformIndex
        fwrite( &m_platformIndex, sizeof( m_platformIndex ), 1, metricFile );

        // m_params
        fwrite( &m_params.Version, sizeof( m_params.Version ), 1, metricFile );

        // m_symbolsVector
        m_symbolSet.WriteSymbolSetToFile( metricFile );

        // m_groupsVector
        uint32_t groupsCount = m_groupsVector.size();
        fwrite( &groupsCount, sizeof( groupsCount ), 1, metricFile );
        for( auto& group : m_groupsVector )
        {
            retVal = group->WriteCConcurrentGroupToFile( metricFile );
            if( retVal != CC_OK )
            {
                break;
            }
        }

        fclose( metricFile );

        return retVal;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     ReadGlobalSymbolsFromFileBuffer
    //
    // Description:
    //     Reads global symbols from file buffer and advances the pointer.
    //
    // Input:
    //     uint8_t*&       bufferPtr             - file buffer
    //     const uint8_t*  fileBufferBeginOffset - file buffer begin offset
    //     const uint32_t  fileSize              - file size
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::ReadGlobalSymbolsFromFileBuffer( uint8_t*& bufferPtr, const uint8_t* fileBufferBeginOffset, const uint32_t fileSize )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, bufferPtr, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, fileBufferBeginOffset, CC_ERROR_INVALID_PARAMETER );

        auto isValidByteArray = []( const TGlobalSymbolLatest& globalSymbol )
        {
            return globalSymbol.SymbolTypedValue.ValueType == VALUE_TYPE_BYTEARRAY && globalSymbol.SymbolTypedValue.ValueByteArray != nullptr;
        };

        TGlobalSymbol   globalSymbol = {};
        TCompletionCode ret          = CC_OK;
        uint32_t        symbolsCount = 0;

        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, symbolsCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );

        for( uint32_t i = 0; i < symbolsCount; ++i )
        {
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, globalSymbol.symbol_1_0.SymbolName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadTTypedValueFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, globalSymbol.symbol_1_0.SymbolTypedValue, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            uint32_t valueSymbolType = 0;
            ret                      = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueSymbolType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            globalSymbol.symbolType = static_cast<TSymbolType>( valueSymbolType );

            if( m_symbolSet.IsSymbolAlreadyAdded( globalSymbol.symbol_1_0.SymbolName ) )
            {
                if( isValidByteArray( globalSymbol.symbol_1_0 ) )
                {
                    DeleteByteArray( globalSymbol.symbol_1_0.SymbolTypedValue.ValueByteArray, adapterId );
                }
                continue;
            }

            ret = m_symbolSet.AddSymbol(
                globalSymbol.symbol_1_0.SymbolName,
                globalSymbol.symbol_1_0.SymbolTypedValue,
                globalSymbol.symbolType );

            if( ret != CC_OK && isValidByteArray( globalSymbol.symbol_1_0 ) )
            {
                DeleteByteArray( globalSymbol.symbol_1_0.SymbolTypedValue.ValueByteArray, adapterId );
            }
        }

        m_params.GlobalSymbolsCount = m_symbolSet.GetSymbolCount();

        return CC_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     ReadConcurrentGroupsFromFileBuffer
    //
    // Description:
    //     Reads concurrent groups from file buffer and advances the pointer.
    //
    // Input:
    //     uint8_t*&               bufferPtr             - file buffer
    //     const uint8_t*          fileBufferBeginOffset - file buffer begin offset
    //     const uint32_t          fileSize              - file size
    //     TApiVersion_1_0*        apiVersion            - API version
    //     const uint32_t          fileVersion           - custom metric file version
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::ReadConcurrentGroupsFromFileBuffer( uint8_t*& bufferPtr, const uint8_t* fileBufferBeginOffset, const uint32_t fileSize, TApiVersion_1_0* apiVersion, const uint32_t fileVersion )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, bufferPtr, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, fileBufferBeginOffset, CC_ERROR_INVALID_PARAMETER );

        TCompletionCode ret                 = CC_OK;
        const char*     symbolicName        = nullptr;
        const char*     shortName           = nullptr;
        uint32_t        measurementTypeMask = 0;
        bool            isSupported         = false;
        uint32_t        groupsCount         = 0;

        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, groupsCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );

        for( uint32_t i = 0; i < groupsCount; ++i )
        {
            // ConcurrentGroupParams
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, symbolicName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, shortName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, measurementTypeMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            auto concurrentGroup = GetConcurrentGroupByName( symbolicName );
            if( concurrentGroup == nullptr )
            {
                uint8_t          platformMaskByteArray[MD_PLATFORM_MASK_BYTE_ARRAY_SIZE] = {};
                TByteArrayLatest platformMask                                            = { MD_PLATFORM_MASK_BYTE_ARRAY_SIZE, platformMaskByteArray };

                ret = SetAllBitsPlatformMask( adapterId, &platformMask );
                MD_CHECK_CC_RET_A( adapterId, ret );

                concurrentGroup = AddConcurrentGroup( symbolicName, shortName, measurementTypeMask, &platformMask, isSupported );
                if( !isSupported )
                {
                    MD_LOG_A( adapterId, LOG_WARNING, "%s concurrent group is not supported!", symbolicName );
                    return CC_ERROR_NOT_SUPPORTED;
                }

                MD_CHECK_PTR_RET_A( adapterId, concurrentGroup, CC_ERROR_NO_MEMORY );
            }

            // MetricSets
            ret = ReadMetricSetsFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, concurrentGroup, apiVersion, fileVersion );
            MD_CHECK_CC_RET_A( adapterId, ret );
        }

        return CC_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     ReadMetricSetsFromFileBuffer
    //
    // Description:
    //     Reads metric sets from file buffer, adds them to the group and advances the pointer.
    //
    // Input:
    //     uint8_t*&                bufferPtr             - file buffer
    //     const uint8_t*           fileBufferBeginOffset - file buffer begin offset
    //     const uint32_t           fileSize              - file size
    //     CConcurrentGroup*        group                 - parent concurrent group
    //     TApiVersion_1_0*         apiVersion            - API version
    //     const uint32_t           fileVersion           - custom metric file version
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::ReadMetricSetsFromFileBuffer( uint8_t*& bufferPtr, const uint8_t* fileBufferBeginOffset, const uint32_t fileSize, CConcurrentGroup* group, TApiVersion_1_0* apiVersion, const uint32_t fileVersion )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, bufferPtr, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, fileBufferBeginOffset, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, group, CC_ERROR_INVALID_PARAMETER );

        TCompletionCode ret                  = CC_OK;
        CMetricSet*     set                  = nullptr;
        CMetricSet*     existingSet          = nullptr;
        const char*     symbolicName         = nullptr;
        const char*     availabilityEquation = nullptr;
        uint32_t        count                = 0;
        uint32_t        metricSetsCount      = 0;

        TMetricSetParams_1_11 metricSetParams;
        TApiSpecificId_1_0    apiSpecificId;
        TReportType           reportType;

        // MetricSets
        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetsCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );

        for( uint32_t j = 0; j < metricSetsCount; ++j )
        {
            set         = nullptr;
            existingSet = nullptr;

            // MetricSetParams
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.SymbolName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.ShortName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.ApiMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.CategoryMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.RawReportSize, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.QueryReportSize, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.PlatformMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            if( ( apiVersion->MajorNumber == MD_API_MAJOR_NUMBER_1 && apiVersion->MinorNumber >= MD_API_MINOR_NUMBER_4 ) || ( apiVersion->MajorNumber > MD_API_MAJOR_NUMBER_1 ) )
            {
                ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricSetParams.GtMask, adapterId );
                MD_CHECK_CC_RET_A( adapterId, ret );
            }
            else
            {
                metricSetParams.GtMask = (uint32_t) GT_TYPE_ALL;
            }

            if( ( apiVersion->MajorNumber == MD_API_MAJOR_NUMBER_1 && apiVersion->MinorNumber >= MD_API_MINOR_NUMBER_11 ) || ( apiVersion->MajorNumber > MD_API_MAJOR_NUMBER_1 ) )
            {
                ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, availabilityEquation, adapterId );
                MD_CHECK_CC_RET_A( adapterId, ret );
            }

            uint32_t valueReportType = 0;
            ret                      = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueReportType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            reportType = static_cast<TReportType>( valueReportType );

            TByteArrayLatest* platformMask = nullptr;
            if( fileVersion >= CUSTOM_METRICS_FILE_VERSION_3 )
            {
                ret = ReadByteArrayFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, platformMask, adapterId );
                MD_CHECK_CC_RET_A( adapterId, ret );
            }
            else
            {
                platformMask = GetByteArrayFromPlatformType( metricSetParams.PlatformMask, MD_PLATFORM_MASK_BYTE_ARRAY_SIZE, adapterId );
            }
            MD_CHECK_PTR_RET_A( adapterId, platformMask, CC_ERROR_GENERAL );

            existingSet = group->GetMatchingMetricSet( metricSetParams.SymbolName, platformMask, metricSetParams.GtMask );
            if( !existingSet )
            {
                set = group->AddMetricSet(
                    metricSetParams.SymbolName,
                    metricSetParams.ShortName,
                    metricSetParams.ApiMask,
                    metricSetParams.CategoryMask,
                    metricSetParams.RawReportSize,
                    metricSetParams.QueryReportSize,
                    reportType,
                    platformMask,
                    availabilityEquation,
                    metricSetParams.GtMask,
                    true );

                if( !set )
                {
                    DeleteByteArray( platformMask, adapterId );
                    MD_CHECK_PTR_RET_A( adapterId, set, CC_ERROR_NO_MEMORY );
                }
                MD_LOG_A( adapterId, LOG_DEBUG, "adding set: %s", metricSetParams.ShortName );
            }
            else
            {
                MD_LOG_A( adapterId, LOG_DEBUG, "set not added, using existing one: %s", metricSetParams.ShortName );
            }
            // platformMask is stored in metric set. iu_memcpy_s used.
            DeleteByteArray( platformMask, adapterId );

            // ApiSpecificId
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.D3D9QueryId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.D3D9Fourcc, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.D3D1XQueryId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.D3D1XDevDependentId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.D3D1XDevDependentName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.OGLQueryIntelId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.OGLQueryIntelName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret )
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.OGLQueryARBTargetId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.OCL, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, apiSpecificId.HwConfigId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( set )
            {
                set->SetApiSpecificId( apiSpecificId );
            }

            // Metrics - if set's been existing, add only missing metrics
            ret = ReadMetricsFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, existingSet ? existingSet : set, existingSet == nullptr );
            MD_CHECK_CC_RET_A( adapterId, ret );

            // Information
            ret = ReadInformationFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, set );
            MD_CHECK_CC_RET_A( adapterId, ret );

            // Start and stop registers
            ret = ReadRegistersFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, set );
            MD_CHECK_CC_RET_A( adapterId, ret );

            // ComplementaryMetricSets
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, count, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            for( uint32_t k = 0; k < count; ++k )
            {
                ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, symbolicName, adapterId );
                MD_CHECK_CC_RET_A( adapterId, ret );
                if( set )
                {
                    set->AddComplementaryMetricSet( symbolicName );
                }
            }
        }

        return CC_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricSet
    //
    // Method:
    //     ReadMetricsFromFileBuffer
    //
    // Description:
    //     Reads metrics from file buffer, adds them to the set and advances the pointer.
    //
    // Input:
    //     uint8_t*&                bufferPtr             - file buffer
    //     const uint8_t*           fileBufferBeginOffset - file buffer begin offset
    //     const uint32_t           fileSize              - file size
    //     CMetricSet*              set                   - parent metric set
    //     const bool               isSetNew              - if true, add all metrics, otherwise add only metrics that
    //                                                      don't exist in the set. It's to prevent adding duplicated
    //                                                      metrics when reading existing set (e.g. default RenderBasic)
    //                                                      with added custom metrics.
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::ReadMetricsFromFileBuffer( uint8_t*& bufferPtr, const uint8_t* fileBufferBeginOffset, const uint32_t fileSize, CMetricSet* set, const bool isSetNew )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, bufferPtr, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, fileBufferBeginOffset, CC_ERROR_INVALID_PARAMETER );

        CMetric*           metric         = nullptr;
        const char*        equationString = nullptr;
        const char*        signalName     = nullptr;
        TMetricParams_1_0  metricParams;
        TDeltaFunction_1_0 deltaFunction;
        bool               skip               = ( set == nullptr );
        TCompletionCode    ret                = CC_OK;
        uint32_t           metricsCount       = 0;
        char*              symbolName         = nullptr;
        int64_t            valueLowWatermark  = 0;
        int64_t            valueHighWatermark = 0;

        // Metrics
        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricsCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );
        for( uint32_t i = 0; i < metricsCount; ++i )
        {
            metric                   = nullptr;
            uint32_t valueResultType = 0;
            uint32_t valueMetricType = 0;
            uint32_t valueHwUnitType = 0;

            // MetricParams
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.GroupId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.SymbolName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.ShortName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.GroupName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.LongName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.DxToOglAlias, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.UsageFlagsMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.ApiMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueResultType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            metricParams.ResultType = static_cast<TMetricResultType>( valueResultType );

            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, metricParams.MetricResultUnits, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueMetricType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            metricParams.MetricType = static_cast<TMetricType>( valueMetricType );

            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueHwUnitType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            metricParams.HwUnitType = static_cast<THwUnitType>( valueHwUnitType );

            ret = ReadInt64FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueLowWatermark, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            metricParams.LowWatermark = static_cast<uint64_t>( valueLowWatermark );

            ret = ReadInt64FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueHighWatermark, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            metricParams.HighWatermark = static_cast<uint64_t>( valueHighWatermark );

            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, signalName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            if( !skip )
            {
                // Add only unique metrics to an existing metric set
                if( isSetNew || !set->IsMetricAlreadyAdded( metricParams.SymbolName ) )
                {
                    metric = set->AddMetric(
                        metricParams.SymbolName,
                        metricParams.ShortName,
                        metricParams.LongName,
                        metricParams.GroupName,
                        metricParams.GroupId,
                        metricParams.UsageFlagsMask,
                        metricParams.ApiMask,
                        metricParams.MetricType,
                        metricParams.ResultType,
                        metricParams.MetricResultUnits,
                        metricParams.LowWatermark,
                        metricParams.HighWatermark,
                        metricParams.HwUnitType,
                        equationString,
                        metricParams.DxToOglAlias,
                        signalName,
                        i,
                        true );
                    MD_CHECK_PTR_RET_A( adapterId, metric, CC_ERROR_NO_MEMORY );
                }
            }

            uint32_t valueFunctionType = 0;

            // Delta function
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueFunctionType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            deltaFunction.FunctionType = static_cast<TDeltaFunctionType>( valueFunctionType );

            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, deltaFunction.BitsCount, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( metric )
            {
                metric->SetSnapshotReportDeltaFunction( deltaFunction );
            }

            // Snapshot report read equation
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( metric )
            {
                metric->SetSnapshotReportReadEquation( equationString );
            }

            // Delta report read equation
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( metric )
            {
                metric->SetDeltaReportReadEquation( equationString );
            }

            // Normalization equation
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( metric )
            {
                metric->SetNormalizationEquation( equationString );
            }

            // Max value equation
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( metric )
            {
                metric->SetMaxValueEquation( equationString );
            }
        }

        return CC_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     ReadInformationFromFileBuffer
    //
    // Description:
    //     Reads information from file buffer, adds them to the set and advances the pointer.
    //
    // Input:
    //     uint8_t*&          bufferPtr             - file buffer
    //     const uint8_t*     fileBufferBeginOffset - file buffer begin offset
    //     const uint32_t     fileSize              - file size
    //     CMetricSet*        set                   - parent metric set
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::ReadInformationFromFileBuffer( uint8_t*& bufferPtr, const uint8_t* fileBufferBeginOffset, const uint32_t fileSize, CMetricSet* set )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, bufferPtr, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, fileBufferBeginOffset, CC_ERROR_INVALID_PARAMETER );

        CInformation*          aInformation   = nullptr;
        const char*            equationString = nullptr;
        TInformationParams_1_0 informationParams;
        TDeltaFunction_1_0     deltaFunction;
        bool                   skip              = ( set == nullptr );
        TCompletionCode        ret               = CC_OK;
        uint32_t               informationCount  = 0;
        uint32_t               valueInfoType     = 0;
        uint32_t               valueFunctionType = 0;

        // Information
        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );
        for( uint32_t k = 0; k < informationCount; ++k )
        {
            aInformation = nullptr;

            // InformationParams
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationParams.SymbolName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationParams.ShortName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationParams.GroupName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationParams.LongName, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationParams.ApiMask, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret                        = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueInfoType, adapterId );
            informationParams.InfoType = static_cast<TInformationType>( valueInfoType );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadCStringFromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, informationParams.InfoUnits, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            if( !skip )
            {
                aInformation = set->AddInformation(
                    informationParams.SymbolName,
                    informationParams.ShortName,
                    informationParams.LongName,
                    informationParams.GroupName,
                    informationParams.ApiMask,
                    informationParams.InfoType,
                    informationParams.InfoUnits,
                    equationString,
                    k );
                MD_CHECK_PTR_RET_A( adapterId, aInformation, CC_ERROR_NO_MEMORY );
            }

            // Delta function
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueFunctionType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            deltaFunction.FunctionType = static_cast<TDeltaFunctionType>( valueFunctionType );

            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, deltaFunction.BitsCount, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( aInformation )
            {
                aInformation->SetOverflowFunction( deltaFunction );
            }

            // Snapshot report read equation
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( aInformation )
            {
                aInformation->SetSnapshotReportReadEquation( equationString );
            }

            // Delta report read equation
            ret = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            if( aInformation )
            {
                aInformation->SetDeltaReportReadEquation( equationString );
            }
        }

        return CC_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     ReadRegistersFromFileBuffer
    //
    // Description:
    //     Reads start and stop registers from file buffer, adds them to the set and
    //     advances the pointer.
    //
    // Input:
    //     uint8_t*&          bufferPtr             - file buffer
    //     const uint8_t*     fileBufferBeginOffset - file buffer begin offset
    //     const uint32_t     fileSize              - file size
    //     CMetricSet*        set                   - parent metric set
    //
    // Output:
    //     TCompletionCode - result of the operation
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::ReadRegistersFromFileBuffer( uint8_t*& bufferPtr, const uint8_t* fileBufferBeginOffset, const uint32_t fileSize, CMetricSet* set )
    {
        const uint32_t adapterId = m_adapter.GetAdapterId();

        MD_CHECK_PTR_RET_A( adapterId, bufferPtr, CC_ERROR_INVALID_PARAMETER );
        MD_CHECK_PTR_RET_A( adapterId, fileBufferBeginOffset, CC_ERROR_INVALID_PARAMETER );

        TRegister          reg;
        const char*        equationString = nullptr;
        TRegisterSetParams registerSetParams;
        uint32_t           regCount        = 0;
        bool               skip            = ( set == nullptr );
        TCompletionCode    ret             = CC_OK;
        uint32_t           regSetCount     = 0;
        uint32_t           valueConfigType = 0;

        // Start register sets
        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, regSetCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );
        for( uint32_t i = 0; i < regSetCount; ++i )
        {
            // RegisterSetParams
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, registerSetParams.ConfigId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, registerSetParams.ConfigPriority, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueConfigType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            registerSetParams.ConfigType = static_cast<TConfigType>( valueConfigType );
            ret                          = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            if( !skip )
            {
                TCompletionCode ret = set->AddStartRegisterSet(
                    registerSetParams.ConfigId,
                    registerSetParams.ConfigPriority,
                    equationString,
                    registerSetParams.ConfigType );
                MD_CHECK_CC_RET_A( adapterId, ret );
            }

            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, regCount, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            for( uint32_t j = 0; j < regCount; j++ )
            {
                if( !skip )
                {
                    reg = *( (TRegister*) ( bufferPtr ) );
                    set->AddStartConfigRegister( reg.offset, reg.value, reg.type );
                }
                MD_CHECK_BUFFER_A( adapterId, bufferPtr, fileBufferBeginOffset, sizeof( TRegister ), fileSize );
                bufferPtr += sizeof( TRegister );
            }
        }

        // Stop register sets - !StopRegisters are obsolete, remains to be backward compatible (in new versions count is always 0)!
        ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, regSetCount, adapterId );
        MD_CHECK_CC_RET_A( adapterId, ret );
        for( uint32_t i = 0; i < regSetCount; ++i )
        {
            // RegisterSetParams
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, registerSetParams.ConfigId, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, registerSetParams.ConfigPriority, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, valueConfigType, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );
            registerSetParams.ConfigType = static_cast<TConfigType>( valueConfigType );
            ret                          = ReadEquationStringFromFile( bufferPtr, fileBufferBeginOffset, fileSize, equationString, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            ret = ReadUInt32FromFileBuffer( bufferPtr, fileBufferBeginOffset, fileSize, regCount, adapterId );
            MD_CHECK_CC_RET_A( adapterId, ret );

            const uint32_t regTotalSize = sizeof( TRegister ) * regCount;
            MD_CHECK_BUFFER_A( adapterId, bufferPtr, fileBufferBeginOffset, regTotalSize, fileSize );
            bufferPtr += regTotalSize;
        }

        if( !skip )
        {
            set->RefreshConfigRegisters();
        }
        return CC_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     OpenFromFile
    //
    // Description:
    //     Opens, checks for MDAPI plain text format and loads file with saved
    //     metrics device if the format is valid.

    //
    // Input:
    //     const char* fileName - file name
    //
    // Output:
    //     TCompletionCode      - result
    //
    //////////////////////////////////////////////////////////////////////////////
    TCompletionCode CMetricsDevice::OpenFromFile( const char* fileName )
    {
        TCompletionCode retVal           = CC_OK;
        FILE*           metricFile       = nullptr;
        uint8_t*        metricFileBuffer = nullptr;
        uint8_t*        bufferPtr        = nullptr;
        uint32_t        fileVersion      = CUSTOM_METRICS_FILE_VERSION_0;
        const uint32_t  adapterId        = m_adapter.GetAdapterId();

        iu_fopen_s( &metricFile, fileName, "rb" );
        MD_CHECK_PTR_RET_A( adapterId, metricFile, CC_ERROR_FILE_NOT_FOUND );

        const uint32_t fileSize = static_cast<uint32_t>( GetFileSize( metricFile, adapterId ) );
        if( fileSize == static_cast<uint32_t>( -1 ) || fileSize < sizeof( MD_METRICS_FILE_KEY ) )
        {
            fclose( metricFile );
            return CC_ERROR_INVALID_PARAMETER;
        }

        metricFileBuffer = new( std::nothrow ) uint8_t[fileSize]();
        if( metricFileBuffer == nullptr )
        {
            fclose( metricFile );
            return CC_ERROR_NO_MEMORY;
        }

        MD_LOG_A( adapterId, LOG_DEBUG, "Check if file is in MDAPI plain text format" );
        if( IsMetricsFileInPlainTextFormat( metricFile, fileVersion ) )
        {
            // Load plain text format file
            if( iu_fread_s( metricFileBuffer, fileSize, 1, fileSize, metricFile ) == 0 )
            {
                MD_LOG_A( adapterId, LOG_DEBUG, "Cannot load file" );
                MD_SAFE_DELETE_ARRAY( metricFileBuffer );
                fclose( metricFile );
                return CC_ERROR_INVALID_PARAMETER;
            }
            bufferPtr = metricFileBuffer;

            if( fileVersion == CUSTOM_METRICS_FILE_VERSION_1 )
            {
                bufferPtr += sizeof( MD_METRICS_FILE_KEY );
            }
            else if( fileVersion == CUSTOM_METRICS_FILE_VERSION_2 )
            {
                bufferPtr += sizeof( MD_METRICS_FILE_KEY_2_0 );
            }
            else if( fileVersion == CUSTOM_METRICS_FILE_VERSION_3 )
            {
                bufferPtr += sizeof( MD_METRICS_FILE_KEY_3_0 );
            }
        }
        else
        {
            MD_LOG_A( adapterId, LOG_ERROR, "Metrics device file is not valid" );
            retVal = CC_ERROR_INVALID_PARAMETER;
        }
        fclose( metricFile );

        if( retVal == CC_OK )
        {
            if( fileVersion > CUSTOM_METRICS_FILE_VERSION_1 )
            {
                uint32_t majorApiVersion = 0;
                uint32_t minorApiVersion = 0;

                retVal = ReadUInt32FromFileBuffer( bufferPtr, metricFileBuffer, fileSize, majorApiVersion, adapterId );
                MD_CHECK_CC( retVal );

                retVal = ReadUInt32FromFileBuffer( bufferPtr, metricFileBuffer, fileSize, minorApiVersion, adapterId );
                MD_CHECK_CC( retVal );

                if( ( majorApiVersion == MD_API_MAJOR_NUMBER_CURRENT && minorApiVersion > MD_API_MINOR_NUMBER_CURRENT ) || majorApiVersion > MD_API_MAJOR_NUMBER_CURRENT )
                {
                    MD_LOG_A( adapterId, LOG_ERROR, "Requered MDAPI version %d.%d, current version %d.%d", majorApiVersion, minorApiVersion, MD_API_MAJOR_NUMBER_CURRENT, MD_API_MINOR_NUMBER_CURRENT );
                    m_isOpenedFromFile = false;
                    MD_SAFE_DELETE_ARRAY( metricFileBuffer );
                    return CC_ERROR_NOT_SUPPORTED;
                }
            }

            if( fileVersion >= CUSTOM_METRICS_FILE_VERSION_3 )
            {
                MD_LOG_A( adapterId, LOG_DEBUG, "Metrics device file saved on platform index: %u, current: %u", *( (uint32_t*) bufferPtr ), m_platformIndex );
                bufferPtr += sizeof( uint32_t );
            }
            else
            {
                uint32_t       platFormIndex = UINT32_MAX;
                const uint32_t platformMask  = *( (TPlatformType*) bufferPtr );
                for( uint32_t i = 0; i < sizeof( platformMask ) * MD_BYTE; ++i )
                {
                    if( platformMask & ( 1 << i ) )
                    {
                        platFormIndex = i;
                        break;
                    }
                }

                if( platFormIndex == UINT32_MAX )
                {
                    MD_LOG_A( adapterId, LOG_DEBUG, "WARNING: read platform mask of metrics device is empty." );
                }
                else
                {
                    MD_LOG_A( adapterId, LOG_DEBUG, "Metrics device file saved on platform: %u, current: %u", platFormIndex, m_platformIndex );
                }

                bufferPtr += sizeof( TPlatformType );
            }

            // MetricsDeviceParams
            TApiVersion_1_0 apiVersion = {};

            retVal = ReadUInt32FromFileBuffer( bufferPtr, metricFileBuffer, fileSize, apiVersion.MajorNumber, adapterId );
            MD_CHECK_CC( retVal );
            retVal = ReadUInt32FromFileBuffer( bufferPtr, metricFileBuffer, fileSize, apiVersion.MinorNumber, adapterId );
            MD_CHECK_CC( retVal );
            retVal = ReadUInt32FromFileBuffer( bufferPtr, metricFileBuffer, fileSize, apiVersion.BuildNumber, adapterId );
            MD_CHECK_CC( retVal );

            MD_LOG_A( adapterId, LOG_DEBUG, "Metrics device file saved with MDAPI v. %d.%d.%d, current v: %d.%d.%d", apiVersion.MajorNumber, apiVersion.MinorNumber, apiVersion.BuildNumber, MD_API_MAJOR_NUMBER_CURRENT, MD_API_MINOR_NUMBER_CURRENT, MD_API_BUILD_NUMBER_CURRENT );

            // GlobalSymbols
            retVal = ReadGlobalSymbolsFromFileBuffer( bufferPtr, metricFileBuffer, fileSize );
            MD_CHECK_CC( retVal );

            // ConcurrentGroup tree
            if( retVal == CC_OK )
            {
                retVal = ReadConcurrentGroupsFromFileBuffer( bufferPtr, metricFileBuffer, fileSize, &apiVersion, fileVersion );
                MD_CHECK_CC( retVal );
            }
        }
        m_isOpenedFromFile = ( retVal == CC_OK );

    exception:
        MD_SAFE_DELETE_ARRAY( metricFileBuffer );
        return retVal;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     IsMetricsFileInPlainTextFormat
    //
    // Description:
    //     Check if metrics file has a proper header with MD_METRICS_FILE_KEY.
    //
    // Input:
    //     FILE*     metricFile  - opened file with custom metrics.
    //     uint32_t& fileVersion - custom file version [out]
    //
    // Output:
    //     bool                  - true if file in MDAPI plain text format, false otherwise
    //
    //////////////////////////////////////////////////////////////////////////////
    bool CMetricsDevice::IsMetricsFileInPlainTextFormat( FILE* metricFile, uint32_t& fileVersion )
    {
        const uint32_t metricFileKeySize              = sizeof( MD_METRICS_FILE_KEY_3_0 );
        uint8_t        readFileKey[metricFileKeySize] = { 0 };

        // Load fragment of the file as plain text
        if( iu_fread_s( readFileKey, metricFileKeySize, 1, metricFileKeySize, metricFile ) == 0 )
        {
            return false;
        }

        // Move file pointer to the start
        rewind( metricFile );

        if( iu_strncmp( MD_METRICS_FILE_KEY, (const char*) &readFileKey, metricFileKeySize ) == 0 )
        {
            fileVersion = CUSTOM_METRICS_FILE_VERSION_1;
        }
        else if( iu_strncmp( MD_METRICS_FILE_KEY_2_0, (const char*) &readFileKey, metricFileKeySize ) == 0 )
        {
            fileVersion = CUSTOM_METRICS_FILE_VERSION_2;
        }
        else if( iu_strncmp( MD_METRICS_FILE_KEY_3_0, (const char*) &readFileKey, metricFileKeySize ) == 0 )
        {
            fileVersion = CUSTOM_METRICS_FILE_VERSION_3;
        }
        else
        {
            fileVersion = CUSTOM_METRICS_FILE_VERSION_0;
        }

        // Check if the file has MDAPI plain text header
        return fileVersion > CUSTOM_METRICS_FILE_VERSION_0;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetDriverInterface
    //
    // Description:
    //     Returns driver interface.
    //
    // Output:
    //     CDriverInterface& - driver interface reference
    //
    //////////////////////////////////////////////////////////////////////////////
    CDriverInterface& CMetricsDevice::GetDriverInterface()
    {
        return m_driverInterface;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetAdapter
    //
    // Description:
    //     Returns underlying adapter reference.
    //
    // Output:
    //     CAdapter& - adapter reference
    //
    //////////////////////////////////////////////////////////////////////////////
    CAdapter& CMetricsDevice::GetAdapter()
    {
        return m_adapter;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetSymbolSet
    //
    // Description:
    //     Returns reference to the symbol set.
    //
    // Output:
    //     CSymbolSet& - reference to the symbol set
    //
    //////////////////////////////////////////////////////////////////////////////
    CSymbolSet& CMetricsDevice::GetSymbolSet()
    {
        return m_symbolSet;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetPlatformByteArray
    //
    // Description:
    //     Returns platform index.
    //
    // Output:
    //     uint32_t - platform index.
    //
    //////////////////////////////////////////////////////////////////////////////
    uint32_t CMetricsDevice::GetPlatformIndex()
    {
        return m_platformIndex;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     IsOpenedFromFile
    //
    // Description:
    //     Returns true if device was opened from a file
    //
    // Output:
    //     bool - true if opened from file
    //
    //////////////////////////////////////////////////////////////////////////////
    bool CMetricsDevice::IsOpenedFromFile()
    {
        return m_isOpenedFromFile;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     ConvertGpuTimestampToNs
    //
    // Description:
    //     Converts gpu timestamp to ns and provides a sync with report timestamps.
    //
    // Input:
    //     const uint64_t gpuTimestampTicks     - gpu timestamp in ticks.
    //     const uint64_t gpuTimestampFrequency - gpu timestamp frequency.
    //
    // Output:
    //     uint64_t                             - gpu timestamp in ns.
    //
    //////////////////////////////////////////////////////////////////////////////
    uint64_t CMetricsDevice::ConvertGpuTimestampToNs( const uint64_t gpuTimestampTicks, const uint64_t gpuTimestampFrequency )
    {
        if( gpuTimestampFrequency == 0 )
        {
            return 0;
        }
        // Ticks masked to 32bit to get sync with report timestamps.
        return ( gpuTimestampTicks & MD_GPU_TIMESTAMP_MASK_32 ) * MD_SECOND_IN_NS / gpuTimestampFrequency;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetReferenceCounter
    //
    // Description:
    //     Returns metrics device reference counters.
    //
    // Output:
    //     uint32_t - metrics device reference counter
    //
    //////////////////////////////////////////////////////////////////////////////
    uint32_t& CMetricsDevice::GetReferenceCounter()
    {
        return m_referenceCounter;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetSubDeviceIndex
    //
    // Description:
    //     Returns sub device index.
    //
    // Output:
    //     uint32_t - sub device index
    //
    //////////////////////////////////////////////////////////////////////////////
    uint32_t CMetricsDevice::GetSubDeviceIndex()
    {
        return m_subDeviceIndex;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetOaBufferCount
    //
    // Description:
    //     Returns oa buffer count.
    //
    // Output:
    //     uint32_t - oa buffer count
    //
    //////////////////////////////////////////////////////////////////////////////
    uint32_t CMetricsDevice::GetOaBufferCount()
    {
        if( m_oaBuferCount == 0 )
        {
            const uint32_t adapterId = m_adapter.GetAdapterId();

            GTDIDeviceInfoParamExtOut out = {};
            const auto                ret = m_driverInterface.SendDeviceInfoParamEscape( GTDI_DEVICE_PARAM_OA_BUFFERS_COUNT, &out, this );
            if( ret == CC_OK )
            {
                m_oaBuferCount = out.ValueUint32;
                MD_LOG_A( adapterId, LOG_DEBUG, "Oa buffer count: %u", m_oaBuferCount );
            }
            else
            {
                MD_LOG_A( adapterId, LOG_ERROR, "ERROR: Unable to get oa buffer count. Return code: %u", ret );
            }
        }

        return m_oaBuferCount;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetStreamConfigId
    //
    // Description:
    //     Returns stream configuration id.
    //
    // Output:
    //     int32_t - configuration id.
    //
    //////////////////////////////////////////////////////////////////////////////
    int32_t CMetricsDevice::GetStreamConfigId()
    {
        return m_streamConfigId;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetStreamId
    //
    // Description:
    //     Returns stream id.
    //
    // Output:
    //     int32_t - stream id.
    //
    //////////////////////////////////////////////////////////////////////////////
    int32_t CMetricsDevice::GetStreamId()
    {
        return m_streamId;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     SetStreamId
    //
    // Description:
    //     Sets stream id.
    //
    // Input:
    //     const int32_t id - stream id.
    //
    //////////////////////////////////////////////////////////////////////////////
    void CMetricsDevice::SetStreamId( const int32_t id )
    {
        m_streamId = id;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     SetStreamConfigId
    //
    // Description:
    //     Sets stream configuration id.
    //
    // Input:
    //     const int32_t id - configuration id.
    //
    //////////////////////////////////////////////////////////////////////////////
    void CMetricsDevice::SetStreamConfigId( const int32_t id )
    {
        m_streamConfigId = id;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetStreamBuffer
    //
    // Description:
    //     Returns preallocated buffer for reading data from tbs stream to avoid new allocations on every read.
    //
    // Output:
    //     std::vector<uint8_t> - tbs stream buffer.
    //
    //////////////////////////////////////////////////////////////////////////////
    std::vector<uint8_t>& CMetricsDevice::GetStreamBuffer()
    {
        return m_streamBuffer;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Class:
    //     CMetricsDevice
    //
    // Method:
    //     GetConcurrentGroupByName
    //
    // Description:
    //     Returns chosen concurrent group by name or nullptr if not exists.
    //
    // Input:
    //     const char* symbolName - name of a concurrent group to look for
    //
    // Output:
    //     CConcurrentGroup*      - found concurrent group or nullptr
    //
    //////////////////////////////////////////////////////////////////////////////
    CConcurrentGroup* CMetricsDevice::GetConcurrentGroupByName( const char* symbolName )
    {
        MD_CHECK_PTR_RET_A( m_adapter.GetAdapterId(), symbolName, nullptr );

        for( auto& group : m_groupsVector )
        {
            if( group && ( strcmp( symbolName, group->GetParams()->SymbolName ) == 0 ) )
            {
                return group;
            }
        }

        return nullptr;
    }
} // namespace MetricsDiscoveryInternal
