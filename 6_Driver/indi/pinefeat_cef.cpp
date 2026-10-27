/*
    Pinefeat EF / EF-S Lens Controller – Canon® Lens Compatible
    Copyright (C) 2025 Pinefeat LLP (support@pinefeat.co.uk)

    Extended set of the Basilisk E-mount lens controller (`t` reports ext=1)
    Copyright (C) 2026 zUrp Astronomics

    Based on Moonlite focuser
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "pinefeat_cef.h"

#include <connectionplugins/connectionserial.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

#define ERR_NC(res) (strcmp(res, "nc") == 0 ? "lens is not attached" : res)

static std::unique_ptr<PinefeatCEF> pinefeatCEF(new PinefeatCEF());

PinefeatCEF::PinefeatCEF()
{
    setVersion(1, 2);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_HAS_VARIABLE_SPEED);

    lastUpdate = std::chrono::steady_clock::now();
}

const char * PinefeatCEF::getDefaultName()
{
    return "Pinefeat EF Lens Controller";
}

bool PinefeatCEF::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMinMax(1, 4);
    FocusSpeedNP[0].setStep(1);
    FocusSpeedNP[0].setValue(1);

    FocusMaxPosNP[0].setMinMax(0, 32767);
    FocusMaxPosNP[0].setStep(1);
    FocusMaxPosNP[0].setValue(0);

    FocusRelPosNP[0].setMinMax(0, 32767);
    FocusRelPosNP[0].setStep(1);
    FocusRelPosNP[0].setValue(0);

    FocusAbsPosNP[0].setMinMax(0, 32767);
    FocusAbsPosNP[0].setStep(1);
    FocusAbsPosNP[0].setValue(0);

    CalibrateSP[0].fill("CALIBRATE", "Calibrate", ISS_OFF);
    CalibrateSP.fill(m_defaultDevice->getDeviceName(), "CALIBRATE", "Calibrate", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                     IPS_OK);

    ApertureAbsNP[0].fill("APERTURE_ABSOLUTE", "f-stop", "%.1f", 0.0, 327.67, 0.0, 0.0);
    ApertureAbsNP.fill(getDeviceName(), "ABS_APERTURE", "Absolute Aperture", MAIN_CONTROL_TAB, IP_WO,
                       60, IPS_OK);

    ApertureRelNP[0].fill("APERTURE_RELATIVE", "f-stop", "%.1f", -327.68, 327.67, 0.0, 0.0);
    ApertureRelNP.fill(getDeviceName(), "REL_APERTURE", "Relative Aperture", MAIN_CONTROL_TAB, IP_WO,
                       60, IPS_OK);

    ApertureRangeTP[0].fill("APERTURE_RANGE", "f-stop", nullptr);
    ApertureRangeTP.fill(getDeviceName(), "RANGE_APERTURE", "Aperture range", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    FocusDistanceTP[0].fill("FOCUS_DISTANCE", "meter", nullptr);
    FocusDistanceTP.fill(getDeviceName(), "FOCUS_DISTANCE", "Focus Distance", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    FirmwareTP[0].fill("FIRMWARE_VERSION", "Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_VERSION", "Firmware", INFO_TAB, IP_RO, 0, IPS_IDLE);

    // Extended set, defined only when `t` reports ext=1
    ApertureNP[0].fill("FNUMBER", "f-stop", "%.1f", 0.0, 327.67, 0.0, 0.0);
    ApertureNP.fill(getDeviceName(), "APERTURE", "Aperture", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    MarkTP[0].fill("MARK", "steps", nullptr);
    MarkTP.fill(getDeviceName(), "FOCUS_MARK", "Focus mark", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    MarkSP[0].fill("MARK_SET", "Set mark here", ISS_OFF);
    MarkSP[1].fill("MARK_GOTO", "Go to mark", ISS_OFF);
    MarkSP[2].fill("MARK_CLEAR", "Clear mark", ISS_OFF);
    MarkSP.fill(getDeviceName(), "FOCUS_MARK_CTL", "Mark", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    LedNP[0].fill("LEVEL", "%", "%.f", 0, 100, 1, 100);
    LedNP.fill(getDeviceName(), "LED_BRIGHTNESS", "Status LED", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    RebootSP[0].fill("REBOOT", "Reboot lens", ISS_OFF);
    RebootSP.fill(getDeviceName(), "LENS_REBOOT", "Lens", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    LensTP[0].fill("LENS_NAME", "Name", nullptr);
    LensTP.fill(getDeviceName(), "LENS_INFO", "Lens", "Lens", IP_RO, 0, IPS_IDLE);

    // The range is the sensor's (SHT40: -45 to 130 °C, the humidity bounded to 0-100 %RH by the board)
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -45, 130, 0, 0);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_OK);

    HumidityNP[0].fill("HUMIDITY", "%RH", "%6.2f", 0, 100, 0, 0);
    HumidityNP.fill(getDeviceName(), "FOCUS_HUMIDITY", "Humidity", MAIN_CONTROL_TAB, IP_RO, 0, IPS_OK);

    // The board's threshold of condensation on the optics: the dew point when it is 0 or above, the frost point below;
    // its range is the one the board computes it in (-65 to 60 °C)
    DewPointNP[0].fill("DEW_POINT", "Celsius (frost point below 0)", "%6.2f", -65, 60, 0, 0);
    DewPointNP.fill(getDeviceName(), "FOCUS_DEW_POINT", "Dew or frost point", MAIN_CONTROL_TAB, IP_RO, 0, IPS_OK);

    RestartTP[0].fill("CAUSE", "Cause", nullptr);
    RestartTP.fill(getDeviceName(), "BOARD_RESTART", "Board restart", INFO_TAB, IP_RO, 0, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    setDefaultPollingPeriod(50);

    return true;
}

bool PinefeatCEF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        positionError.clear();

        defineProperty(FirmwareTP);
        defineProperty(FocusDistanceTP);
        defineProperty(CalibrateSP);
        defineProperty(ApertureRangeTP);
        defineProperty(ApertureAbsNP);
        defineProperty(ApertureRelNP);

        int32_t pos, max;
        std::string dist, aper;
        if (readFocusPosition(pos)
                && (max = pos, 1)
                && readFocusMaxPosition(max)
                && readFocusDistance(dist)
                && readApertureRange(aper))
        {
            updateProperties(pos, max, dist, aper);
            LOG_INFO("Parameters updated, the controller is ready for use.");
        }

        if (ext)
        {
            defineProperty(ApertureNP);
            defineProperty(MarkTP);
            defineProperty(MarkSP);
            defineProperty(RebootSP);
            defineProperty(LensTP);
            defineProperty(LedNP);
            defineProperty(RestartTP);

            // The board keeps no LED level (100 at each start): the saved one is sent again
            loadConfig(true, LedNP.getName());
            readLed(IPS_OK);
            readExtended();
        }
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(FocusDistanceTP);
        deleteProperty(CalibrateSP);
        deleteProperty(ApertureRangeTP);
        deleteProperty(ApertureAbsNP);
        deleteProperty(ApertureRelNP);

        if (ext)
        {
            deleteProperty(ApertureNP);
            deleteProperty(MarkTP);
            deleteProperty(MarkSP);
            deleteProperty(RebootSP);
            deleteProperty(LensTP);
            deleteProperty(LedNP);
            deleteProperty(RestartTP);
            climateProperties(false);
        }
    }

    return true;
}

void PinefeatCEF::updateProperties(const int32_t pos, const int32_t max, const std::string dist, const std::string aper)
{
    FocusAbsPosNP[0].setValue(pos);
    // Extended set: the state a move ended in (TimerHit) stays until the next one; only a Busy ends here
    if (!ext || FocusAbsPosNP.getState() == IPS_BUSY)
        FocusAbsPosNP.setState(IPS_OK);
    FocusAbsPosNP.apply();

    if (FocusRelPosNP.getState() == IPS_BUSY)
    {
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
    }

    if (FocusMaxPosNP.getState() == IPS_BUSY)
    {
        FocusMaxPosNP[0].setValue(max);
        FocusMaxPosNP.setState(IPS_IDLE);
        FocusMaxPosNP.apply();

        double values[] = { FocusMaxPosNP[0].getValue() };
        char *names[] = { (char*)FocusMaxPosNP[0].getName() };
        ISNewNumber(getDeviceName(), FocusMaxPosNP.getName(), values, names, 1);
    }

    FocusDistanceTP[0].setText(dist);
    FocusDistanceTP.apply();

    ApertureRangeTP[0].setText(aper);
    ApertureRangeTP.apply();
}

bool PinefeatCEF::Handshake()
{
    for (int i = 0; i < 3; i++)
    {
        if (readFirmwareVersion())
        {
            // Extended set: only a controller that reports ext=1 to `t` gets it (a Pinefeat controller answers `er`)
            char res[EXT_BUF] = {0};
            ext = false;
            if (sendCommand("t\n", res, EXT_BUF))
            {
                std::istringstream line(res);
                std::string token;
                while (line >> token)
                    ext = ext || token == "ext=1";
            }

            uint32_t cap = FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_HAS_VARIABLE_SPEED;
            FI::SetCapability(ext ? cap | FOCUSER_CAN_ABORT : cap);

            // The range before any bounds of `r`, in this connection too: a past lens is not this one, and a Pinefeat
            // controller gets the range of initProperties
            if (limitsApplied)
                resetLimits();

            tracked = relMove = rebooting = stopWatch = false;
            if (ext)
            {
                LOG_INFO("Extended set enabled (ext=1).");
                // A move's end state stays until the next one (updateProperties): none from a past connection
                FocusAbsPosNP.setState(IPS_OK);
                // Nor a restart: the first `x` of this connection is one (readRestart), even of the cause and the
                // time the last connection read
                restartKnown = false;
            }

            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(getCurrentPollingPeriod()));
    }

    LOG_ERROR("Can't detect the controller, please ensure the device is powered and the port is correct.");
    return false;
}

bool PinefeatCEF::readFirmwareVersion()
{
    // Extended set: only a line of the form of a version is the reply (sendCommand), read into EXT_BUF. Opening the
    // port can restart the Basilisk board, and the lines of its ROM, some longer than CEF_BUF, come after `v` was sent.
    char res[EXT_BUF] = {0};

    if (!sendCommand("v\n", res, EXT_BUF, true))
        return false;

    int major, minor;
    if (sscanf(res, "%d.%d", &major, &minor) == 2)
    {
        firmwareMinor = minor;
    }

    FirmwareTP[0].setText(res);
    FirmwareTP.setState(IPS_OK);

    LOGF_INFO("Detected firmware version %s.", res);

    return true;
}

bool PinefeatCEF::readFocusPosition(int32_t &pos)
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("f\n", res))
        return false;

    int rc = sscanf(res, "%d", &pos);
    if (rc <= 0)
    {
        // Logged once, not at each refresh while it lasts (no lens: `nc` until one is attached)
        if (positionError != res)
            LOGF_ERROR("Can't read focus position: %s.", ERR_NC(res));
        positionError = res;
        return false;
    }

    positionError.clear();
    return true;
}

bool PinefeatCEF::readFocusMaxPosition(int32_t &pos)
{
    if (firmwareMinor < 3)
    {
        return true;
    }

    char res[CEF_BUF] = {0};

    if (!sendCommand("r\n", res))
        return false;

    int rc = sscanf(res, "%*d-%d", &pos);
    if (rc <= 0)
    {
        LOGF_ERROR("Can't read max focus position: %s.", ERR_NC(res));
        return false;
    }

    // Extended set: both bounds are the lens's own, the range of the position (applyLimits)
    int32_t min;
    if (ext && sscanf(res, "%d-", &min) == 1)
        applyLimits(min, pos);

    return true;
}

// Extended set: the bounds of `r` are the lens's own (published by the lens, nothing measured), and the range of the
// absolute position and of FOCUS_MAX, whose value is the high bound: a client is not offered a position the board
// refuses (`er range limits`). Set at each read of `r` that succeeds, sent to the clients only when they changed: a new
// lens, or a FOCUS_MAX written by a client or the config, which INDI::Focuser turns into an absolute range from 0. A
// read that fails changes nothing: the last known bounds stay.
void PinefeatCEF::applyLimits(int32_t min, int32_t max)
{
    limitsApplied = true;
    if (FocusAbsPosNP[0].getMin() == min && FocusAbsPosNP[0].getMax() == max
            && FocusMaxPosNP[0].getMin() == min && FocusMaxPosNP[0].getMax() == max
            && FocusMaxPosNP[0].getValue() == max)
        return;

    FocusAbsPosNP[0].setMinMax(min, max);
    FocusAbsPosNP.updateMinMax();

    FocusMaxPosNP[0].setMinMax(min, max);
    FocusMaxPosNP[0].setValue(max);
    FocusMaxPosNP.setState(IPS_OK);
    FocusMaxPosNP.updateMinMax();

    LOGF_INFO("Lens focus range %d-%d.", min, max);
}

// Extended set: the range of the absolute position and of FOCUS_MAX that initProperties sets, the one before any
// bounds of `r`
void PinefeatCEF::resetLimits()
{
    FocusMaxPosNP[0].setMinMax(0, 32767);
    FocusMaxPosNP[0].setValue(0);
    FocusAbsPosNP[0].setMinMax(0, 32767);
    limitsApplied = false;
}

bool PinefeatCEF::readFocusDistance(std::string &result)
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("d\n", res))
        return false;

    result = res;
    return true;
}

bool PinefeatCEF::readApertureRange(std::string &result)
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("a\n", res))
        return false;

    result = res;
    return true;
}

bool PinefeatCEF::isNotMoving()
{
    char res[CEF_BUF] = {0};
    return sendCommand("e\n", res) && strstr(res, "n");
}

bool PinefeatCEF::moveFocusAbs(uint32_t position)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "f%u\n", position);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't focus: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::moveFocusRel(FocusDirection dir, uint32_t offset)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "f%s%u\n", (dir == FOCUS_INWARD) ? "-" : "+", offset);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't focus: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::setSpeed(int speed)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "s%d\n", speed);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't set speed: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::setApertureAbs(double value)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "a%.6g\n", value);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't set aperture: %s.", ERR_NC(res));
        return false;
    }

    LOGF_INFO("Aperture is set to f/%.6g.", value);

    return true;
}

bool PinefeatCEF::setApertureRel(double value)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "a%s%.6g\n", (value < 0) ? "" : "+", value);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't set aperture: %s.", ERR_NC(res));
        return false;
    }

    LOGF_INFO("Iris is %s by f/%.6g further.", (value > 0) ? "closed" : "opened", abs(value));

    return true;
}

bool PinefeatCEF::calibrate()
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("c\n", res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't calibrate: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

// Extended set: `x`, `o`, `j`, `i` and `z`, at the connection, after a mark command, and at each refresh while still
// (`x` and `z` while it moves too: TimerHit)
void PinefeatCEF::readExtended()
{
    char res[EXT_BUF] = {0};

    readRestart();

    if (!sendCommand("o\n", res, EXT_BUF))
        return;
    double fnum;
    if (sscanf(res, "%lf", &fnum) == 1)
    {
        ApertureNP[0].setValue(fnum);
        ApertureNP.setState(IPS_OK);
    }
    else
        ApertureNP.setState(IPS_IDLE);     // an `er …`: the lens serves no aperture, or it is not known
    ApertureNP.apply();

    if (!sendCommand("j\n", res, EXT_BUF))
        return;
    showExtText(MarkTP, res);

    if (!sendCommand("i\n", res, EXT_BUF))
        return;
    showExtText(LensTP, res);

    readClimate();
}

// Extended set, `x`: the board's last restart, `<cause> <ms since it>`, shown as its cause. The board can restart
// while the port stays open, and it resumes its lens after a crash or a watchdog: the focus moves. A restart is the
// first reply of a connection, then a reply whose cause differs from the last one or whose time is less than it can
// be without a restart (it starts from 0 at each one: two restarts of the same cause differ, however close). One that
// is not normal (a crash, a watchdog, the supply, a cause the board doesn't know) is logged as a warning, once per
// restart; `poweron`, `sw` (the board's own restart), `usb` (opening the port can restart it) and `jtag` are not. A
// reply that can't be read changes nothing.
void PinefeatCEF::readRestart()
{
    char res[EXT_BUF] = {0};
    char cause[EXT_BUF] = {0};
    unsigned long long ms;
    auto sent = std::chrono::steady_clock::now();
    if (!sendCommand("x\n", res, EXT_BUF) || sscanf(res, "%255s %llu", cause, &ms) != 2)
        return;
    auto received = std::chrono::steady_clock::now();

    // Without a restart, the board's time has grown since the last reply by at least the time from that reply's
    // arrival to this command's sending, however late either reply came: the board reads its clock after it gets the
    // command and before its reply arrives. Less 10 ms and 0.1 % of that time: the board's time is truncated to the
    // ms, and its clock and the host's don't run at quite the same rate.
    bool same = false;
    if (restartKnown && restartCause == cause)
    {
        unsigned long long gap = std::chrono::duration_cast<std::chrono::milliseconds>(sent - restartRead).count();
        same = ms + 10 + gap / 1000 >= restartTime + gap;
    }
    restartKnown = true;
    restartCause = cause;
    restartTime = ms;
    restartRead = received;
    if (same)
        return;

    const char * why = nullptr;
    if (restartCause == "panic")
        why = "its firmware crashed";
    else if (restartCause == "int_wdt")
        why = "its interrupt watchdog fired";
    else if (restartCause == "task_wdt")
        why = "its task watchdog fired";
    else if (restartCause == "wdt")
        why = "a hardware watchdog fired";
    else if (restartCause == "brownout")
        why = "its supply voltage dropped";
    else if (restartCause != "poweron" && restartCause != "sw" && restartCause != "usb" && restartCause != "jtag")
        why = "its cause is unknown";

    RestartTP[0].setText(restartCause);
    RestartTP.setState(why == nullptr ? IPS_OK : IPS_ALERT);
    RestartTP.apply();
    if (why != nullptr)
        LOGF_WARN("The board restarted: %s (%s).", why, cause);
}

// Extended set: a reply as a text field. An `er …` is never shown as a value: the field is empty. The state: Ok with a
// value; Alert when the value exists but can't be read now (`er nolens`, `er busy boot`, `er fault`); Idle when the
// board doesn't serve it (`er nocap`)
void PinefeatCEF::showExtText(INDI::PropertyText &property, const char * res)
{
    bool error = strncmp(res, "er", 2) == 0 && (res[2] == ' ' || res[2] == '\0');
    property[0].setText(error ? "" : res);
    property.setState(!error ? IPS_OK : strncmp(res, "er nocap", 8) == 0 ? IPS_IDLE : IPS_ALERT);
    property.apply();
}

// Extended set, `z`: `<°C> <%RH> <threshold>` while the board knows the temperature and the humidity, `-` while it
// doesn't (no reading yet, the sensor silent, a bad checksum, the bus stuck, a reading too old). The threshold is the
// one the board computes, published as it comes, never computed here: the dew point when it is 0 or above, the frost
// point below; `-` when the board doesn't define it (no humidity, the air above 60 °C, a frost point below -65 °C).
// Ekos takes any value published on FOCUS_TEMPERATURE as it comes, whatever the property's state: an unknown value is
// therefore never published, not even as an old number in an Alert state. Each property exists only while the board
// reports its value: defined when one comes, deleted when it is unknown or unread; a client sees no number rather than
// a stale one. Any other reply is unknown. Nothing is compensated.
void PinefeatCEF::readClimate()
{
    char res[EXT_BUF] = {0};
    char cond[EXT_BUF] = {0};
    double t, rh;
    if (!sendCommand("z\n", res, EXT_BUF) || sscanf(res, "%lf %lf %255s", &t, &rh, cond) != 3)
    {
        climateProperties(false);
        return;
    }

    TemperatureNP[0].setValue(t);
    HumidityNP[0].setValue(rh);
    if (climateDefined)
    {
        TemperatureNP.apply();
        HumidityNP.apply();
    }
    else
        climateProperties(true);

    // The threshold: a number, whole, or the board's `-`
    char *end = nullptr;
    double dew = std::strtod(cond, &end);
    if (end == cond || *end != '\0')
    {
        dewPointProperty(false);
        return;
    }
    DewPointNP[0].setValue(dew);
    if (dewPointDefined)
        DewPointNP.apply();
    else
        dewPointProperty(true);
}

// Extended set: TemperatureNP and HumidityNP defined (their values set before) or deleted, each change once;
// DewPointNP goes with them when they are deleted
void PinefeatCEF::climateProperties(bool define)
{
    if (!define)
        dewPointProperty(false);
    if (define == climateDefined)
        return;
    climateDefined = define;
    if (define)
    {
        defineProperty(TemperatureNP);
        defineProperty(HumidityNP);
        LOG_INFO("Board temperature and humidity known.");
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(HumidityNP);
        LOG_INFO("Board temperature and humidity unknown: not published.");
    }
}

// Extended set: DewPointNP defined (its value set before) or deleted, each change once
void PinefeatCEF::dewPointProperty(bool define)
{
    if (define == dewPointDefined)
        return;
    dewPointDefined = define;
    if (define)
    {
        defineProperty(DewPointNP);
        LOG_INFO("Board dew or frost point known.");
    }
    else
    {
        deleteProperty(DewPointNP);
        LOG_INFO("Board dew or frost point unknown: not published.");
    }
}

// Extended set, `k`: the value shown is the one the board reads back
void PinefeatCEF::readLed(IPState state)
{
    char res[EXT_BUF] = {0};
    int level;

    if (sendCommand("k\n", res, EXT_BUF) && sscanf(res, "%d", &level) == 1)
        LedNP[0].setValue(level);
    else
        state = IPS_ALERT;

    LedNP.setState(state);
    LedNP.apply();
}

bool PinefeatCEF::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CalibrateSP.isNameMatch(name))
        {
            CalibrateSP.reset();

            // Extended set: nothing to calibrate, the bounds of `r` are the lens's own (the board answers `ok` to `c`
            // and does nothing). CALIBRATE answers them: `r` read again and applied (readFocusMaxPosition), `c` not
            // sent, nothing moves, no Busy. `r` failing (no lens): Alert, the known bounds stay.
            if (ext)
            {
                int32_t max;
                CalibrateSP.setState(readFocusMaxPosition(max) ? IPS_OK : IPS_ALERT);
                CalibrateSP.apply();
                return true;
            }

            if (calibrate())
            {
                // Delay further update until calibration starter
                lastUpdate = std::chrono::steady_clock::now();

                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply();

                FocusMaxPosNP.setState(IPS_BUSY);
                FocusMaxPosNP.apply();

                CalibrateSP.setState(IPS_OK);
            }
            else
                CalibrateSP.setState(IPS_ALERT);

            CalibrateSP.apply();
            return true;
        }

        if (ext && MarkSP.isNameMatch(name))
        {
            MarkSP.update(states, names, n);
            int i = MarkSP.findOnSwitchIndex();
            MarkSP.reset();

            const char * cmd = i == 0 ? "js\n" : i == 1 ? "jg\n" : i == 2 ? "jx\n" : nullptr;
            char res[EXT_BUF] = {0};
            if (cmd != nullptr && sendCommand(cmd, res, EXT_BUF) && strstr(res, "ok"))
            {
                if (i == 1)
                {
                    moveStarted(false);
                    FocusAbsPosNP.setState(IPS_BUSY);
                    FocusAbsPosNP.apply();
                }
                MarkSP.setState(IPS_OK);
            }
            else
            {
                if (cmd != nullptr)
                    LOGF_ERROR("Can't %s: %s.", i == 0 ? "set the mark" : i == 1 ? "go to the mark" : "clear the mark",
                               res);
                MarkSP.setState(IPS_ALERT);
            }

            MarkSP.apply();
            readExtended();
            return true;
        }

        if (ext && RebootSP.isNameMatch(name))
        {
            RebootSP.reset();

            char res[EXT_BUF] = {0};
            if (sendCommand("b\n", res, EXT_BUF) && strstr(res, "ok"))
            {
                LOG_INFO("Lens reboot requested.");
                rebooting = true;
                stopWatch = false;
                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply();
                RebootSP.setState(IPS_OK);
            }
            else
            {
                LOGF_ERROR("Can't reboot the lens: %s.", res);
                RebootSP.setState(IPS_ALERT);
            }

            RebootSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool PinefeatCEF::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ApertureAbsNP.isNameMatch(name))
        {
            ApertureAbsNP.update(values, names, n);

            bool res = setApertureAbs(ApertureAbsNP[0].getValue());
            if (res)
                ApertureAbsNP.setState(IPS_OK);
            else
                ApertureAbsNP.setState(IPS_ALERT);

            ApertureAbsNP.apply();
            return res;
        }

        if (ApertureRelNP.isNameMatch(name))
        {
            ApertureRelNP.update(values, names, n);

            bool res = setApertureRel(ApertureRelNP[0].getValue());
            if (res)
                ApertureRelNP.setState(IPS_OK);
            else
                ApertureRelNP.setState(IPS_ALERT);

            ApertureRelNP.apply();
            return res;
        }

        if (ext && LedNP.isNameMatch(name))
        {
            LedNP.update(values, names, n);

            char cmd[CEF_BUF] = {0};
            char res[EXT_BUF] = {0};
            snprintf(cmd, CEF_BUF, "k%ld\n", lround(LedNP[0].getValue()));

            bool ok = sendCommand(cmd, res, EXT_BUF) && strstr(res, "ok");
            if (!ok)
                LOGF_ERROR("Can't set the LED brightness: %s.", res);

            readLed(ok ? IPS_OK : IPS_ALERT);
            return ok;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool PinefeatCEF::SetFocuserSpeed(int speed)
{
    return setSpeed(speed);
}

// Extended set, `q`
bool PinefeatCEF::AbortFocuser()
{
    // Without ext=1, a focuser that can't abort, as INDI::Focuser makes it: no `q`
    if (!ext)
        return INDI::Focuser::AbortFocuser();

    char res[EXT_BUF] = {0};
    if (!sendCommand("q\n", res, EXT_BUF))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't abort: %s.", res);
        return false;
    }

    return true;
}

bool PinefeatCEF::saveConfigItems(FILE * fp)
{
    INDI::Focuser::saveConfigItems(fp);

    if (ext)
        LedNP.save(fp);

    return true;
}

IPState PinefeatCEF::MoveAbsFocuser(uint32_t targetTicks)
{
    if (!moveFocusAbs(targetTicks))
        return IPS_ALERT;

    moveStarted(false);
    return IPS_BUSY;
}

IPState PinefeatCEF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    if (!moveFocusRel(dir, ticks))
        return IPS_ALERT;

    moveStarted(true);
    return IPS_BUSY;
}

// Extended set: a move the board accepted (absolute, relative, `jg`) is tracked to its end, read by `g`; it ends the
// watch of the previous stop
void PinefeatCEF::moveStarted(bool rel)
{
    if (!ext)
        return;
    tracked = true;
    relMove = rel;
    stopWatch = false;
}

// Extended set, once `e` answers n after a tracked move: its end, as the state of the focuser position. `g`: `ok` Ok;
// `aborted`, the user's own `q` (AbortFocuser: the barrel button sends none), is no failure: Idle, the state
// INDI::Focuser gives a successful abort; anything else Alert, logged. After `stall` or `aborted`, the stop is watched
// (watchStop). A position `f` can't read is Alert whatever `g` says.
IPState PinefeatCEF::moveEnd(bool positionRead)
{
    std::string g = readMoveResult();
    IPState state = IPS_ALERT;

    if (g == "ok")
        state = IPS_OK;
    else if (g == "aborted")
        state = IPS_IDLE;
    else if (g == "unconfirmed")
        LOG_ERROR("Stop not confirmed: the motor state is unknown.");
    else
        LOGF_ERROR("Focus move failed: %s.", g.empty() ? "no reply to g" : g.c_str());

    if (g == "stall" || g == "aborted")
    {
        // `unconfirmed` comes at most 4.5 s after the stop was first sent: 5 s, a margin
        stopWatch = true;
        stopWatchFrom = g;
        stopWatchEnd = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }

    return positionRead ? state : IPS_ALERT;
}

// Extended set: `g` read at each timer pass while the stop is watched. `unconfirmed` turns the position to Alert; any
// other change of `g` (a move the barrel button launched has ended) or the end of the window ends the watch, and the
// state of the move's end stays.
void PinefeatCEF::watchStop()
{
    if (std::chrono::steady_clock::now() >= stopWatchEnd)
    {
        stopWatch = false;
        return;
    }

    std::string g = readMoveResult();
    if (g.empty() || g == stopWatchFrom)
        return;

    stopWatch = false;
    if (g != "unconfirmed")
        return;

    LOG_ERROR("Stop not confirmed: the motor state is unknown.");
    FocusAbsPosNP.setState(IPS_ALERT);
    FocusAbsPosNP.apply();
    if (relMove)
    {
        FocusRelPosNP.setState(IPS_ALERT);
        FocusRelPosNP.apply();
    }
}

// Extended set, `g`: the result of the last tracked move; empty if the board did not reply
std::string PinefeatCEF::readMoveResult()
{
    char res[EXT_BUF] = {0};
    if (!sendCommand("g\n", res, EXT_BUF))
        return "";
    return res;
}

void PinefeatCEF::TimerHit()
{
    if (!isConnected())
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();

    if (ext && stopWatch)
        watchStop();

    // Once a second: the next refresh is a second after this one
    if (elapsed >= 1)
        lastUpdate = now;

    if (elapsed >= 1 && isNotMoving())
    {
        int32_t pos, max;
        std::string dist, aper;
        bool positionRead = readFocusPosition(pos);
        bool read = positionRead
                    && (max = pos, 1)
                    && readFocusMaxPosition(max)
                    && readFocusDistance(dist)
                    && readApertureRange(aper);

        // Extended set: a Busy it set ends here, `e` at n: a tracked move by `g`, `b` by `f` answering; Alert if `f`
        // doesn't (no lens, `er fault`)
        if (ext && (tracked || rebooting))
        {
            IPState state = tracked ? moveEnd(positionRead) : positionRead ? IPS_OK : IPS_ALERT;
            tracked = rebooting = false;

            FocusAbsPosNP.setState(state);
            if (!read)
                FocusAbsPosNP.apply();
            if (FocusRelPosNP.getState() == IPS_BUSY)
            {
                FocusRelPosNP.setState(state);
                FocusRelPosNP.apply();
            }
        }

        if (read)
        {
            updateProperties(pos, max, dist, aper);
        }
        else
        {
            // Stopped, and the state can't be read (no lens, a fault): a move or a calibration still Busy has
            // failed. Only a successful read ends it otherwise.
            if (FocusAbsPosNP.getState() == IPS_BUSY)
            {
                FocusAbsPosNP.setState(IPS_ALERT);
                FocusAbsPosNP.apply();
            }

            if (FocusRelPosNP.getState() == IPS_BUSY)
            {
                FocusRelPosNP.setState(IPS_ALERT);
                FocusRelPosNP.apply();
            }

            if (FocusMaxPosNP.getState() == IPS_BUSY)
            {
                FocusMaxPosNP.setState(IPS_ALERT);
                FocusMaxPosNP.apply();
            }
        }

        if (ext)
            readExtended();
    }
    // Extended set: `x` and `z` while it moves too (`e` at y, or no reply): a board that restarted resumes its lens,
    // and the warning comes while the focus moves; the board's sensor goes on, and a value it no longer knows is not
    // kept published through a long move
    else if (ext && elapsed >= 1)
    {
        readRestart();
        readClimate();
    }

    SetTimer(getCurrentPollingPeriod());
}

// One byte from fd, waited for until deadline at most: TTY_TIME_OUT once it has passed
static int readByte(int fd, char * c, std::chrono::steady_clock::time_point deadline)
{
    auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
    if (left <= 0)
        return TTY_TIME_OUT;

    int nbytes_read = 0;
    return tty_read_expanded(fd, c, 1, left / 1000000, left % 1000000, &nbytes_read);
}

bool PinefeatCEF::sendCommand(const char * cmd, char * res, int size, bool version)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    // The input still pending is stale (a reply that came after its command's timeout): it is read and discarded,
    // as tcflush did, and if it ends inside a line, the rest of that line, still to come, is not this command's
    // reply either. Only what is already received is read: a controller that keeps sending can't hold the driver here.
    bool midLine = false;
    int pending = 0;
    if (ioctl(PortFD, FIONREAD, &pending) != 0)
        pending = 0;
    while (pending > 0)
    {
        char stale[CEF_BUF];
        ssize_t n = read(PortFD, stale, std::min(pending, int(sizeof(stale))));
        if (n <= 0)
            break;
        pending -= n;
        midLine = stale[n - 1] != CEF_DEL;
    }
    tcflush(PortFD, TCOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
    {
        tcdrain(PortFD);
        return true;
    }

    // The reply has CEF_TIMEOUT in all, from the command, up to its delimiter and at most size bytes, as before:
    // each byte is waited for only until the deadline, so that a controller that sends without a delimiter, however
    // slowly, can't hold the driver longer
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(int(CEF_TIMEOUT));
    // Extended set: a line of the Basilisk board's log (`* …`, after a LOG ON left by its bench page) is never the
    // reply, nor, when version is set, a line that is not a version: each is skipped up to its delimiter, within the
    // same deadline. A Pinefeat controller sends neither: no reply of its starts with '*', and its reply to `v` is its
    // version.
    char c = 0;
    for (;;)
    {
        // The rest of a stale line, or of a line of the log, first
        while (midLine)
        {
            if ((rc = readByte(PortFD, &c, deadline)) != TTY_OK)
            {
                char errstr[MAXRBUF] = {0};
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("Serial read error: %s.", errstr);
                return false;
            }
            midLine = c != CEF_DEL;
        }

        nbytes_read = 0;
        do
        {
            if ((rc = nbytes_read < size ? readByte(PortFD, res + nbytes_read, deadline) : int(TTY_OVERFLOW)) != TTY_OK)
            {
                char errstr[MAXRBUF] = {0};
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("Serial read error: %s.", errstr);
                return false;
            }
        }
        while (res[0] != '*' && res[nbytes_read++] != CEF_DEL);

        if (res[0] == '*')
        {
            midLine = true;
            continue;
        }

        // The reply as a string, without its line end
        while (nbytes_read > 0 && (res[nbytes_read - 1] == CEF_DEL || res[nbytes_read - 1] == '\r'))
            res[--nbytes_read] = '\0';

        int major, minor;
        if (!version || sscanf(res, "%d.%d", &major, &minor) == 2)
            break;
        LOGF_DEBUG("Not a version, skipped: <%s>", res);
    }

    LOGF_DEBUG("RES <%s>", res);

    return true;
}
