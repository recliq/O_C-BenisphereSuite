// Copyright (c) 2018, Jason Justian
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

class DualScale : public HemisphereApplet {
public:

    const char* applet_name() {
        return "DualScale";
    }

    void Start() {
        ForEachChannel(ch) {
            mask[ch] = 0xffff;
            continuous[ch] = 1;
            quantizer[ch].Init();
            quantizer[ch].Configure(OC::Scales::GetScale(5), mask[0]);
        }
        last_scale = 0;
        adc_lag_countdown = 0;
    }

    void Controller() {
        // Prepare to read pitch and send gate in the near future; there's a slight
        // lag between when a gate is read and when the CV can be read.
        ForEachChannel(ch) {
            if (Clock(ch)) {
              continuous[ch] = 0; // Turn off continuous mode if there's a clock
              StartADCLag(ch);
            }

            if (continuous[ch] || EndOfADCLag(ch)) {
                int32_t pitch = In(ch);
                int32_t quantized = quantizer[ch].Process(pitch, 0, 0);
                Out(ch, quantized);
            }
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawKeyboard();
        DrawMaskIndicators();
    }

    void OnButtonPress() {
        uint8_t scale = cursor > 11 ? 1 : 0;
        uint8_t bit = cursor - (scale * 12);

        // Toggle the mask bit at the cursor position
        mask[scale] ^= (0x01 << bit);
        if (scale == last_scale) quantizer[scale].Configure(OC::Scales::GetScale(5), mask[scale]);
        continuous[scale] = 1; // Re-enable continuous mode when scale is changed
    }

     void OnEncoderMove(int direction) {
        if (cursor == 0 && direction == -1) cursor = 1;
        cursor = constrain(cursor += direction, 0, 23);
        ResetCursor();
    }
        
    uint32_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,12}, mask[0]);
        Pack(data, PackLocation {12,12}, mask[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        mask[0] = Unpack(data, PackLocation {0,12});
        mask[1] = Unpack(data, PackLocation {12,12});

        ForEachChannel(ch) quantizer[ch].Configure(OC::Scales::GetScale(5), mask[ch]);
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 1=Ch1 2=Ch2";
        help[HEMISPHERE_HELP_CVS]      = "1=CV 1=Ch1 2=Ch2";
        help[HEMISPHERE_HELP_OUTS]     = "A=Pitch A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER]  = "T=Note P=Toggle";
        //                               "------------------" <-- Size Guide
    }
    
private:
    braids::Quantizer quantizer[2];
    uint16_t mask[2];
    uint8_t cursor; // 0-11=Scale 1; 12-23=Scale 2
    uint8_t last_scale; // The most-recently-used scale (used to set the mask when necessary)
    bool continuous[2]; // Each channel starts as continuous and becomes clocked when a clock is received
    int adc_lag_countdown;

    void DrawKeyboard() {
        // Border
        gfxFrame(0, 27, 63, 32);

        // White keys
        for (uint8_t x = 0; x < 8; x++) {
            gfxLine(x * 8, 27, x * 8, 58);
        }

        // Black keys
        for (uint8_t i = 0; i < 6; i++) {
            if (i != 2) { // Skip the third position
                uint8_t x = (i * 8) + 6;
                gfxRect(x, 27, 5, 16);
            }
        }

        // Which scale
        gfxPrint(10, 15, "Scale ");
        gfxPrint(cursor < 12 ? 1 : 2);
    }

    void DrawMaskIndicators() {
        uint8_t scale = cursor < 12 ? 0 : 1;
        uint8_t x[12] = {2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50};
        uint8_t p[12] = {0, 1,  0,  1,  0,  0,  1,  0,  1,  0,  1,  0};
        for (uint8_t i = 0; i < 12; i++) {
            if ((mask[scale] >> i) & 0x01) gfxInvert(x[i], (p[i] ? 37 : 51), 4 - p[i], 4 - p[i]);
            if (i == (cursor - (scale * 12))) gfxCursor(x[i] - 1, p[i] ? 25 : 60, 6);
        }

        // If C is selcted, display a selector on the higher C, too
        if (mask[scale] & 0x01) gfxInvert(58, 51, 3, 4);
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to ScaleDuet,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
DualScale DualScale_instance[2];

void DualScale_Start(bool hemisphere) {
    DualScale_instance[hemisphere].BaseStart(hemisphere);
}

void DualScale_Controller(bool hemisphere, bool forwarding) {
    DualScale_instance[hemisphere].BaseController(forwarding);
}

void DualScale_View(bool hemisphere) {
    DualScale_instance[hemisphere].BaseView();
}

void DualScale_OnButtonPress(bool hemisphere) {
    DualScale_instance[hemisphere].OnButtonPress();
}

void DualScale_OnEncoderMove(bool hemisphere, int direction) {
    DualScale_instance[hemisphere].OnEncoderMove(direction);
}

void DualScale_ToggleHelpScreen(bool hemisphere) {
    DualScale_instance[hemisphere].HelpScreen();
}

uint64_t DualScale_OnDataRequest(bool hemisphere) {
    return DualScale_instance[hemisphere].OnDataRequest();
}

void DualScale_OnDataReceive(bool hemisphere, uint64_t data) {
    DualScale_instance[hemisphere].OnDataReceive(data);
}

