#ifndef ASX_PROFILE_V2_H
#define ASX_PROFILE_V2_H

#include <stdint.h>

/**
 * @brief Categorias embutidas (Built-in) de perfis do ASXNanoStream V2.
 * Perfis de 1 a 100 são estáticos e não requerem pré-provisionamento na Nuvem,
 * garantindo compatibilidade retroativa e "plug and play".
 */
enum class AsxProfileType : uint32_t
{
    Unknown = 0,

    // Perfil Clima Básico: Temp e Umid (0.1 graus e 0.1 %)
    ClimateBasic_V1 = 1,

    // Perfil Monitoramento Industrial (Temp, Vibração, Bateria)
    IndustrialMonitor_V1 = 2,

    // Rastreador Simples / Contato Seco (Presença e Alarme)
    TrackerDryContact_V1 = 3,

    // Estação Meteorológica Completa / Barométrica (Temp, Umid, Pressão Barométrica, Bateria)
    WeatherEnvironmental_V1 = 4,

    // Monitor de Alimentação / Bateria (Tensão mV, Corrente mA, Carga %, Status Carga)
    PowerBatteryMonitor_V1 = 5,

    // Agro / Estação de Solo (Umidade Solo, Temp Solo, Condutividade Elétrica, Bateria)
    SmartAgroSoil_V1 = 6,

    // Hidrometria / Nível e Vazão de Tanque (Nível cm, Vazão L/min, Volume Acumulado L, Alarme)
    SmartWaterTankFlow_V1 = 7,

    // Rastreador Veicular e Ativos / GPS (Lat, Long, Velocidade, Ignição, Bateria)
    AssetTrackerGps_V1 = 8,

    // Medição de Energia Elétrica AC (Tensão V, Corrente A, Potência Ativa W, Fator Potência)
    SmartEnergyMeter_V1 = 9,

    // Qualidade do Ar e Gases (CO2 ppm, TVOC ppb, PM2.5, Temp, Umid)
    AirQualityGas_V1 = 10
};

/**
 * @brief Tipos numéricos definidos contratualmente
 */
enum class AsxDataType : uint8_t
{
    Boolean = 0,
    Int16 = 1,
    UInt16 = 2,
    Int32 = 3,
    UInt32 = 4,
    Float32 = 5
};

/**
 * @brief Estrutura de Limites para validação e packing de sub-bytes.
 * Estas configurações imutáveis blindam o encoder e o decoder contra lixo
 * no payload e permitem o cálculo matemático de ranges.
 */
struct AsxChannelConstraints
{
    AsxDataType DataType;
    float Scale;             // Divisor/Multiplicador para leitura
    float MinLimit;          // Menor valor aceitável
    float MaxLimit;          // Maior valor aceitável
};

/**
 * @brief Metadados de um perfil embutido no firmware (zero alocação dinâmica).
 */
struct AsxProfileDescriptor
{
    AsxProfileType ProfileType;
    uint32_t ProfileId;
    uint32_t Version;
    uint8_t ChannelCount;
    const AsxChannelConstraints* Channels;
};

namespace AsxBuiltInProfiles
{
    // =========================================================================
    // Profile ID 1: ClimateBasic_V1
    // =========================================================================
    const AsxChannelConstraints ClimateBasic_Channels[] = {
        { AsxDataType::Int16,  0.1f, -100.0f, +150.0f }, // Ch0: Temperatura (0.1 °C)
        { AsxDataType::UInt16, 0.1f,    0.0f, +100.0f }  // Ch1: Umidade (0.1 %)
    };
    const AsxChannelConstraints ClimateBasic_Ch0_Temp = ClimateBasic_Channels[0];
    const AsxChannelConstraints ClimateBasic_Ch1_Hum  = ClimateBasic_Channels[1];

    // =========================================================================
    // Profile ID 2: IndustrialMonitor_V1
    // =========================================================================
    const AsxChannelConstraints Industrial_Channels[] = {
        { AsxDataType::Int16,  0.1f,  -100.0f, +200.0f }, // Ch0: Temperatura (°C)
        { AsxDataType::UInt16, 0.01f,    0.0f, +500.0f }, // Ch1: Vibração RMS (0.01 mm/s)
        { AsxDataType::UInt16, 0.01f,    0.0f, +100.0f }  // Ch2: Tensão Bateria (0.01 V)
    };
    const AsxChannelConstraints Industrial_Ch0_Temp = Industrial_Channels[0];
    const AsxChannelConstraints Industrial_Ch1_Vib  = Industrial_Channels[1];
    const AsxChannelConstraints Industrial_Ch2_Bat  = Industrial_Channels[2];

    // =========================================================================
    // Profile ID 3: TrackerDryContact_V1
    // =========================================================================
    const AsxChannelConstraints Tracker_Channels[] = {
        { AsxDataType::Boolean, 1.0f, 0.0f, 1.0f }, // Ch0: Presença / Contato Seco
        { AsxDataType::Boolean, 1.0f, 0.0f, 1.0f }  // Ch1: Botão Pânico / Alarme
    };
    const AsxChannelConstraints Tracker_Ch0_Presence = Tracker_Channels[0];
    const AsxChannelConstraints Tracker_Ch1_PanicBtn = Tracker_Channels[1];

    // =========================================================================
    // Profile ID 4: WeatherEnvironmental_V1 (Compatível com PressureHectoPascalX10)
    // =========================================================================
    const AsxChannelConstraints Weather_Channels[] = {
        { AsxDataType::Int16,  0.1f,  -100.0f,  +150.0f }, // Ch0: Temperatura (°C)
        { AsxDataType::UInt16, 0.1f,     0.0f,  +100.0f }, // Ch1: Umidade (%)
        { AsxDataType::UInt16, 0.1f,   500.0f, +1200.0f }, // Ch2: Pressão Atmosférica (0.1 hPa)
        { AsxDataType::UInt16, 0.01f,    0.0f,   +10.0f }  // Ch3: Bateria (0.01 V)
    };
    const AsxChannelConstraints Weather_Ch0_Temp     = Weather_Channels[0];
    const AsxChannelConstraints Weather_Ch1_Hum      = Weather_Channels[1];
    const AsxChannelConstraints Weather_Ch2_Pressure = Weather_Channels[2];
    const AsxChannelConstraints Weather_Ch3_Bat      = Weather_Channels[3];

    // =========================================================================
    // Profile ID 5: PowerBatteryMonitor_V1 (Compatível com BatteryMilliVolts)
    // =========================================================================
    const AsxChannelConstraints PowerBattery_Channels[] = {
        { AsxDataType::UInt16, 1.0f,      0.0f, +6000.0f }, // Ch0: Tensão Bateria (mV)
        { AsxDataType::Int16,  0.1f,  -5000.0f, +5000.0f }, // Ch1: Corrente (0.1 mA)
        { AsxDataType::UInt16, 0.1f,      0.0f,  +100.0f }, // Ch2: Carga Percentual (0.1 %)
        { AsxDataType::Boolean, 1.0f,     0.0f,     1.0f }  // Ch3: Status de Carga / Carregando
    };
    const AsxChannelConstraints PowerBattery_Ch0_Voltage = PowerBattery_Channels[0];
    const AsxChannelConstraints PowerBattery_Ch1_Current = PowerBattery_Channels[1];
    const AsxChannelConstraints PowerBattery_Ch2_Soc     = PowerBattery_Channels[2];
    const AsxChannelConstraints PowerBattery_Ch3_Charge  = PowerBattery_Channels[3];

    // =========================================================================
    // Profile ID 6: SmartAgroSoil_V1
    // =========================================================================
    const AsxChannelConstraints AgroSoil_Channels[] = {
        { AsxDataType::UInt16, 0.1f,     0.0f,  +100.0f }, // Ch0: Umidade Solo VWC (0.1 %)
        { AsxDataType::Int16,  0.1f,   -40.0f,   +80.0f }, // Ch1: Temperatura Solo (0.1 °C)
        { AsxDataType::UInt16, 1.0f,     0.0f, +20000.0f }, // Ch2: Condutividade Elétrica (µS/cm)
        { AsxDataType::UInt16, 0.01f,    0.0f,    +5.0f }  // Ch3: Bateria (0.01 V)
    };
    const AsxChannelConstraints AgroSoil_Ch0_Moisture = AgroSoil_Channels[0];
    const AsxChannelConstraints AgroSoil_Ch1_Temp     = AgroSoil_Channels[1];
    const AsxChannelConstraints AgroSoil_Ch2_EC       = AgroSoil_Channels[2];
    const AsxChannelConstraints AgroSoil_Ch3_Bat      = AgroSoil_Channels[3];

    // =========================================================================
    // Profile ID 7: SmartWaterTankFlow_V1
    // =========================================================================
    const AsxChannelConstraints WaterTank_Channels[] = {
        { AsxDataType::UInt16, 0.1f,  0.0f,      +5000.0f }, // Ch0: Nível Reservatório (0.1 cm)
        { AsxDataType::UInt16, 0.1f,  0.0f,      +1000.0f }, // Ch1: Vazão Instantânea (0.1 L/min)
        { AsxDataType::UInt32, 1.0f,  0.0f, 4294967295.0f }, // Ch2: Volume Total (Litros)
        { AsxDataType::Boolean, 1.0f, 0.0f,          1.0f }  // Ch3: Alarme Transbordamento/Vazamento
    };
    const AsxChannelConstraints WaterTank_Ch0_Level    = WaterTank_Channels[0];
    const AsxChannelConstraints WaterTank_Ch1_FlowRate = WaterTank_Channels[1];
    const AsxChannelConstraints WaterTank_Ch2_Volume   = WaterTank_Channels[2];
    const AsxChannelConstraints WaterTank_Ch3_Alarm    = WaterTank_Channels[3];

    // =========================================================================
    // Profile ID 8: AssetTrackerGps_V1
    // =========================================================================
    const AsxChannelConstraints AssetTracker_Channels[] = {
        { AsxDataType::Int32,  0.000001f,  -90.0f,  +90.0f }, // Ch0: Latitude (Graus)
        { AsxDataType::Int32,  0.000001f, -180.0f, +180.0f }, // Ch1: Longitude (Graus)
        { AsxDataType::UInt16, 0.1f,          0.0f, +300.0f }, // Ch2: Velocidade (0.1 km/h)
        { AsxDataType::Boolean, 1.0f,         0.0f,    1.0f }, // Ch3: Ignição / Movimento
        { AsxDataType::UInt16, 0.01f,         0.0f,  +30.0f }  // Ch4: Bateria (0.01 V)
    };
    const AsxChannelConstraints AssetTracker_Ch0_Lat     = AssetTracker_Channels[0];
    const AsxChannelConstraints AssetTracker_Ch1_Lon     = AssetTracker_Channels[1];
    const AsxChannelConstraints AssetTracker_Ch2_Speed   = AssetTracker_Channels[2];
    const AsxChannelConstraints AssetTracker_Ch3_Ignition = AssetTracker_Channels[3];
    const AsxChannelConstraints AssetTracker_Ch4_Bat     = AssetTracker_Channels[4];

    // =========================================================================
    // Profile ID 9: SmartEnergyMeter_V1
    // =========================================================================
    const AsxChannelConstraints EnergyMeter_Channels[] = {
        { AsxDataType::UInt16, 0.1f,  0.0f,   +500.0f }, // Ch0: Tensão RMS (0.1 V)
        { AsxDataType::UInt16, 0.01f, 0.0f,   +100.0f }, // Ch1: Corrente RMS (0.01 A)
        { AsxDataType::UInt32, 1.0f,  0.0f, 100000.0f }, // Ch2: Potência Ativa (Watts)
        { AsxDataType::UInt16, 0.01f, 0.0f,      1.0f }  // Ch3: Fator de Potência (0.01)
    };
    const AsxChannelConstraints EnergyMeter_Ch0_Volt  = EnergyMeter_Channels[0];
    const AsxChannelConstraints EnergyMeter_Ch1_Curr  = EnergyMeter_Channels[1];
    const AsxChannelConstraints EnergyMeter_Ch2_Power = EnergyMeter_Channels[2];
    const AsxChannelConstraints EnergyMeter_Ch3_PF    = EnergyMeter_Channels[3];

    // =========================================================================
    // Profile ID 10: AirQualityGas_V1
    // =========================================================================
    const AsxChannelConstraints AirQuality_Channels[] = {
        { AsxDataType::UInt16, 1.0f,     0.0f, +10000.0f }, // Ch0: CO2 (ppm)
        { AsxDataType::UInt16, 1.0f,     0.0f, +60000.0f }, // Ch1: TVOC (ppb)
        { AsxDataType::UInt16, 0.1f,     0.0f,  +1000.0f }, // Ch2: PM2.5 (0.1 µg/m³)
        { AsxDataType::Int16,  0.1f,   -40.0f,  +125.0f }, // Ch3: Temperatura (0.1 °C)
        { AsxDataType::UInt16, 0.1f,     0.0f,  +100.0f }  // Ch4: Umidade (0.1 %)
    };
    const AsxChannelConstraints AirQuality_Ch0_CO2  = AirQuality_Channels[0];
    const AsxChannelConstraints AirQuality_Ch1_TVOC = AirQuality_Channels[1];
    const AsxChannelConstraints AirQuality_Ch2_PM25 = AirQuality_Channels[2];
    const AsxChannelConstraints AirQuality_Ch3_Temp = AirQuality_Channels[3];
    const AsxChannelConstraints AirQuality_Ch4_Hum  = AirQuality_Channels[4];

    // =========================================================================
    // Descritores Estáticos para Consulta O(1) sem Heap
    // =========================================================================
    const AsxProfileDescriptor BuiltInDescriptors[] = {
        { AsxProfileType::ClimateBasic_V1,         1, 1, 2, ClimateBasic_Channels },
        { AsxProfileType::IndustrialMonitor_V1,    2, 1, 3, Industrial_Channels },
        { AsxProfileType::TrackerDryContact_V1,    3, 1, 2, Tracker_Channels },
        { AsxProfileType::WeatherEnvironmental_V1, 4, 1, 4, Weather_Channels },
        { AsxProfileType::PowerBatteryMonitor_V1,  5, 1, 4, PowerBattery_Channels },
        { AsxProfileType::SmartAgroSoil_V1,        6, 1, 4, AgroSoil_Channels },
        { AsxProfileType::SmartWaterTankFlow_V1,   7, 1, 4, WaterTank_Channels },
        { AsxProfileType::AssetTrackerGps_V1,      8, 1, 5, AssetTracker_Channels },
        { AsxProfileType::SmartEnergyMeter_V1,     9, 1, 4, EnergyMeter_Channels },
        { AsxProfileType::AirQualityGas_V1,       10, 1, 5, AirQuality_Channels }
    };

    const uint8_t BuiltInDescriptorsCount = sizeof(BuiltInDescriptors) / sizeof(BuiltInDescriptors[0]);

    inline const AsxProfileDescriptor* getProfileDescriptor(AsxProfileType profileType)
    {
        uint32_t id = static_cast<uint32_t>(profileType);
        if (id >= 1 && id <= BuiltInDescriptorsCount)
        {
            return &BuiltInDescriptors[id - 1];
        }
        return nullptr;
    }
}

#endif // ASX_PROFILE_V2_H
