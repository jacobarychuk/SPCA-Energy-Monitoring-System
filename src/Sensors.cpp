#include <Sensors.h>
#include <numeric>

extern tm timeData;
Sample collectSample();
void dispatchSample(const Sample&);

// Required initializations
OneWire tempProbe::oneWire(ONE_WIRE_BUS);
DallasTemperature tempProbe::sensors(&oneWire);
std::array<tempProbe, 5> tempProbe::probes = {tempProbe(GLYCOL_ADDR), tempProbe(PREHEAT_ADDR), tempProbe(AMBIENT_ADDR), tempProbe(SOURCE_ADDR), tempProbe(HOT_ADDR)};
short tempProbe::indexRealTime = 0; // Holds the most recent 20 samples (20 samples x 6 seconds = 2 minutes of data)
short tempProbe::indexHourly = 0;   // Holds the most recent 30 maximum temperatures, where each is calculated from the 20 samples in `realTime`
short tempProbe::indexDaily = 0;    // Holds the most recent 24 maximum temperatures from the 30 entries in `hourly`

// Constructor definition
tempProbe::tempProbe(const uint8_t *address)
{
    uniqueAddress = address;
    realTime.fill(0);
    hourly.fill(0);
    daily.fill(0);
}

// Called every 6 seconds by the loop
void tempProbe::readAllProbes()
{
    Sample s = collectSample();
    dispatchSample(s);
    incrementRealTime();
}

Sample collectSample() {
    Sample s;
    s.timestamp = time(nullptr);

    // Read and store values from temperature sensors
    tempProbe::sensors.requestTemperatures();
    for (int i = 0; i < NUM_TEMP_PROBES; i++) {
        auto temp = tempProbe::probes[i].sensors.getTempC(tempProbe::probes[i].uniqueAddress);
        s.temperatures[i] = static_cast<short>(temp * 100);
    }

    // Read and store flow rate
    s.flowRate = flowMeter::instance.getFlowRate();
    return s;
}

// Called every 6 seconds
void tempProbe::incrementRealTime()
{
    indexRealTime++;

    // Reset index once it reaches the end (this creates a wraparound/circular buffer)
    if (indexRealTime == 20)
    {
        indexRealTime = 0;
        updateHourlyData();
    }
}

// Called once every 20 real-time entries (every 20 x 6 = 120 seconds)
void tempProbe::updateHourlyData()
{
    for (auto &probe : probes)
    {
        probe.hourly[indexHourly] = *std::max_element(probe.realTime.begin(), probe.realTime.end());
    }
    flowMeter::instance.hourly[indexHourly] = static_cast<short>(std::accumulate(flowMeter::instance.realTime.begin(), flowMeter::instance.realTime.end(), 0.0) / 10.0);
    indexHourly++;

    // Reset index once it reaches the end (this creates a wraparound/circular buffer)
    if (indexHourly == 30)
    {
        indexHourly = 0;
        updateDailyData();
    }
}

// Called once every 30 hourly entries (every 30 x 2 = 60 minutes)
void tempProbe::updateDailyData()
{
    for (auto &probe : probes)
    {
        probe.daily.at(indexDaily) = *std::max_element(probe.hourly.begin(), probe.hourly.end());
    }
    flowMeter::instance.daily[indexDaily] = static_cast<short>(std::accumulate(flowMeter::instance.hourly.begin(), flowMeter::instance.hourly.end(), 0.0));
    indexDaily++;

    // Reset index once it reaches the end (this creates a wraparound/circular buffer)
    if (indexDaily == 24)
    {
        indexDaily = 0;
    }
}

/*
** When fetching data from the arrays, we need to get the data from the previous index, since the current index is what will be written to next.
*/
String tempProbe::getRealTimeTemp()
{
    String data = "";
    for (auto &probe : probes)
    {
        try
        {
            data += String(probe.realTime.at(indexRealTime - 1)) + ",";
        }
        catch (const std::out_of_range &oor)
        {
            data += String(probe.realTime.at(19)) + ","; // We are on index 0, so we want the last element of the array
        }
    }
    data.remove(data.length() - 1); // remove the final comma
    return data;
}

String tempProbe::getHourlyTemp()
{
    String data = "";
    for (auto &probe : probes)
    {
        try
        {
            data += String(probe.hourly.at(indexHourly - 1)) + ",";
        }
        catch (const std::out_of_range &oor)
        {
            data += String(probe.hourly.at(19)) + ","; // We are on index 0, so we want the last element of the array
        }
    }
    data.remove(data.length() - 1); // remove the final comma
    return data;
}

String tempProbe::getRealTimePower()
{
    String data = "";
    try
    {
        auto solarEnergy = static_cast<short>(static_cast<float>(probes.at(1).realTime.at(indexRealTime - 1)-probes.at(3).realTime.at(indexRealTime-1)) * 4.186 * static_cast<float>(flowMeter::instance.realTime.at(indexRealTime - 1)) / 100.0 / 60.0);
        auto tankEnergy = static_cast<short>(static_cast<float>(probes.at(4).realTime.at(indexRealTime - 1)-probes.at(1).realTime.at(indexRealTime-1)) * 4.186 * static_cast<float>(flowMeter::instance.realTime.at(indexRealTime - 1)) / 100.0 / 60.0);
        data += String(solarEnergy) + "," + String(tankEnergy);
    }
    catch (const std::out_of_range &oor)
    {
        auto solarEnergy = static_cast<short>(static_cast<float>(probes.at(1).realTime.at(19)-probes.at(3).realTime.at(19)) * 4.186 * static_cast<float>(flowMeter::instance.realTime.at(19)) / 100.0 / 60.0);
        auto tankEnergy = static_cast<short>(static_cast<float>(probes.at(4).realTime.at(19)-probes.at(1).realTime.at(19)) * 4.186 * static_cast<float>(flowMeter::instance.realTime.at(19)) / 100.0 / 60.0);
        data += String(solarEnergy) + "," + String(tankEnergy);
    }
    return data;
}

String tempProbe::getHourlyEnergy()
{
    String data = "";
    try
    {
        auto solarEnergy = static_cast<short>(static_cast<float>(probes.at(1).hourly.at(indexHourly - 1)-probes.at(3).hourly.at(indexHourly-1)) * 4.186 * static_cast<float>(flowMeter::instance.hourly.at(indexHourly - 1)) / 100.0 / 3600.0);
        auto tankEnergy = static_cast<short>(static_cast<float>(probes.at(4).hourly.at(indexHourly - 1)-probes.at(1).hourly.at(indexHourly-1)) * 4.186 * static_cast<float>(flowMeter::instance.hourly.at(indexHourly - 1)) / 100.0 / 3600.0);
        data += String(solarEnergy) + "," + String(tankEnergy);
    }
    catch (const std::out_of_range &oor)
    {
        auto solarEnergy = static_cast<short>(static_cast<float>(probes.at(1).hourly.at(19)-probes.at(3).hourly.at(19)) * 4.186 * static_cast<float>(flowMeter::instance.hourly.at(19)) / 100.0 / 3600.0);
        auto tankEnergy = static_cast<short>(static_cast<float>(probes.at(4).hourly.at(19)-probes.at(1).hourly.at(19)) * 4.186 * static_cast<float>(flowMeter::instance.hourly.at(19)) / 100.0 / 3600.0);
        data += String(solarEnergy) + "," + String(tankEnergy);
    }
    return data;
}

void dispatchSample(const Sample& s)
{
    // Store in real-time buffer
    for (int i = 0; i < NUM_TEMP_PROBES; i++) {
        tempProbe::probes[i].realTime[tempProbe::indexRealTime] = s.temperatures[i];
    }

    // Write to CSV
    String data = String(s.timestamp) + ",";
    for (short temperature: s.temperatures) data += String(static_cast<float>(temperature / 100.0)) + ",";
    data += String(static_cast<float>(s.flowRate / 100.0));

    auto fileHandle = SPIFFS.open("/historical_data.csv", FILE_APPEND);
    if (!fileHandle)
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (!fileHandle.println(data))
        Serial.println("Failed to append file");
    else
        Serial.println("File appended");
    fileHandle.close();
}

// Required initialization
flowMeter flowMeter::instance{};

// Constructor definition
flowMeter::flowMeter()
{
    pulses = 0;
    realTime.fill(0);
    hourly.fill(0);
    daily.fill(0);
}

String flowMeter::getRealTimeFlow()
{
    String data = "";
    try
    {
        data += String(instance.realTime.at(tempProbe::indexRealTime - 1));
    }
    catch (const std::out_of_range &oor)
    {
        data += String(instance.realTime.at(19)); // We are on index 0, so we want the last element of the array
    }
    return data;
}

String flowMeter::getHourlyFlow()
{
    String data = "";
    try
    {
        data += String(instance.hourly.at(tempProbe::indexHourly - 1));
    }
    catch (const std::out_of_range &oor)
    {
        data += String(instance.hourly.at(19)); // We are on index 0, so we want the last element of the array
    }
    return data;
}

//Callback function of the interrupt
void IRAM_ATTR pulseCounter()
{
    flowMeter::instance.pulses++;
}

short flowMeter::getFlowRate()
{
    instance.pulses = 0;

    //Count the pulses in 1 second
    attachInterrupt(FLOW_METER_PIN, pulseCounter, FALLING);
    delay(1000);
    detachInterrupt(FLOW_METER_PIN);

    //Flow rate in L/min = (pulses / 5.5), mulitply by 100 to store in short as two-decimal-point representation
    return static_cast<short>(instance.pulses / 5.5 * 100);
}