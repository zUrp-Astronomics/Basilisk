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

#pragma once

#include "indifocuser.h"
#include <chrono>

class PinefeatCEF : public INDI::Focuser
{
    public:
        PinefeatCEF();
        ~PinefeatCEF() override = default;

        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;
        void TimerHit() override;
        bool saveConfigItems(FILE * fp) override;

    protected:
        const char * getDefaultName() override;
        bool Handshake() override;
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        bool SetFocuserSpeed(int speed) override;
        bool AbortFocuser() override;

    private:
        void updateProperties(const int32_t pos, const int32_t max, const std::string dist, const std::string aper);
        bool readFocusPosition(int32_t &pos);
        bool readFocusMaxPosition(int32_t &pos);
        bool readFocusDistance(std::string &result);
        bool readApertureRange(std::string &result);
        bool readFirmwareVersion();
        bool isNotMoving();
        bool moveFocusAbs(uint32_t position);
        bool moveFocusRel(FocusDirection dir, uint32_t offset);
        bool setSpeed(int speed);
        bool setApertureAbs(double value);
        bool setApertureRel(double value);
        bool calibrate();

        // Extended set
        void applyLimits(int32_t min, int32_t max);
        void resetLimits();
        void readExtended();
        void showExtText(INDI::PropertyText &property, const char * res);
        void readClimate();
        void climateProperties(bool define);
        void dewPointProperty(bool define);
        void readLed(IPState state);
        std::string readMoveResult();
        IPState moveEnd(bool positionRead);
        void watchStop();
        void moveStarted(bool rel);

        /**
         * @brief sendCommand Send a string command to the controller.
         * @param cmd Command to be sent, must already have the necessary delimiter ('\n')
         * @param res If not nullptr, the function will read until it detects the default delimiter ('\n') up to
         *        size length, and returns the reply as a string, without its line end. If nullptr, no read back is
         *        done and the function returns true.
         * @param size The size of res, CEF_BUF by default.
         * @param version If true, only a line of the form of a version (`<major>.<minor>`) is the reply.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr, int size = CEF_BUF, bool version = false);

        INDI::PropertySwitch CalibrateSP {1};

        INDI::PropertyNumber ApertureRelNP {1};

        INDI::PropertyNumber ApertureAbsNP {1};

        INDI::PropertyText ApertureRangeTP {1};

        INDI::PropertyText FocusDistanceTP {1};

        INDI::PropertyText FirmwareTP {1};

        // Extended set, defined only when `t` reports ext=1
        INDI::PropertyNumber ApertureNP {1};    // `o`
        INDI::PropertyText MarkTP {1};          // `j`
        INDI::PropertySwitch MarkSP {3};        // `js` `jg` `jx`
        INDI::PropertyNumber LedNP {1};         // `k`
        INDI::PropertySwitch RebootSP {1};      // `b`
        INDI::PropertyText LensTP {1};          // `i`
        INDI::PropertyNumber TemperatureNP {1}; // `z`: FOCUS_TEMPERATURE, the convention Ekos reads
        INDI::PropertyNumber HumidityNP {1};    // `z`
        INDI::PropertyNumber DewPointNP {1};    // `z`: the threshold the board computes

        // CEF Buffer Size
        static const uint8_t CEF_BUF { 16 };

        // CEF Command Delimiter
        static const char CEF_DEL { '\n' };

        // CEF Command Timeout
        static const uint8_t CEF_TIMEOUT { 3 };

        // Buffer of the replies of the extended set, some longer than CEF_BUF (`t`, `i`)
        static const int EXT_BUF { 256 };

        std::chrono::steady_clock::time_point lastUpdate;

        uint16_t firmwareMinor {0};

        // The last reply to `f` that was not a position, logged once; empty once a position is read
        std::string positionError;

        // Extended set: `t` reported ext=1 at the connection
        bool ext {false};

        // The bounds of `r` are the range of the absolute position and of FOCUS_MAX (applyLimits), since the last
        // resetLimits
        bool limitsApplied {false};

        // A Busy the extended set has set, ended in TimerHit once `e` answers n
        bool tracked {false};       // a move the driver launched (absolute, relative, `jg`): its end is read by `g`
        bool relMove {false};       // that move was relative: FocusRelPosNP takes its end too
        bool rebooting {false};     // `b`: its end is `e` at n and `f` answering

        // `z`: TemperatureNP and HumidityNP are defined only while the board reports a value, DewPointNP only while it
        // reports its threshold (readClimate)
        bool climateDefined {false};
        bool dewPointDefined {false};

        // After `stall` or `aborted`, `g` is read again until stopWatchEnd: the stop may turn `unconfirmed`
        bool stopWatch {false};
        std::string stopWatchFrom;
        std::chrono::steady_clock::time_point stopWatchEnd;
};
