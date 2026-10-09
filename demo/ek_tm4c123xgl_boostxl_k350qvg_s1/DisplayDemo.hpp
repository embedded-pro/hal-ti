#ifndef DEMO_EK_TM4C123XGL_BOOSTXL_K350QVG_S1_DISPLAY_DEMO_HPP
#define DEMO_EK_TM4C123XGL_BOOSTXL_K350QVG_S1_DISPLAY_DEMO_HPP

#include "hal/interfaces/Display.hpp"
#include "services/tracer/Tracer.hpp"
#include <array>
#include <cstdint>

namespace demo
{
    class DisplayDemo
    {
    public:
        explicit DisplayDemo(services::Tracer& tracer);

        void Start(hal::Display& display);
        void NextPattern();

    private:
        enum class Pattern : uint8_t
        {
            colorBars,
            greyRamp,
            red,
            green,
            blue,
            count
        };

        void Draw();
        void BuildStrip();
        uint16_t PixelAt(uint16_t x) const;
        void DrawNextStrip();
        void StripDone();
        const char* PatternName() const;

    private:
        static constexpr uint16_t width = 320;
        static constexpr uint16_t height = 240;
        static constexpr uint16_t stripHeight = 8;

        services::Tracer& tracer;
        hal::Display* display = nullptr;
        std::array<uint8_t, width * stripHeight * 2> strip{};
        Pattern pattern = Pattern::colorBars;
        uint16_t nextRow = 0;
        bool drawing = false;
        bool restart = false;
    };
}

#endif
