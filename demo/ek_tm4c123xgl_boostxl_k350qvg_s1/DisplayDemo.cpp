#include "DisplayDemo.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace demo
{
    namespace
    {
        constexpr std::array<uint16_t, 8> colorBars{ { 0xffff, 0xffe0, 0x07ff, 0x07e0, 0xf81f, 0xf800, 0x001f, 0x0000 } };

        constexpr uint16_t Grey(uint8_t level)
        {
            return static_cast<uint16_t>(((level >> 3) << 11) | ((level >> 2) << 5) | (level >> 3));
        }
    }

    DisplayDemo::DisplayDemo(services::Tracer& tracer)
        : tracer(tracer)
    {}

    void DisplayDemo::Start(hal::Display& newDisplay)
    {
        really_assert(newDisplay.Size() == (hal::DisplaySize{ width, height }));
        really_assert(newDisplay.Format() == hal::PixelFormat::rgb565Swapped);

        display = &newDisplay;
        Draw();
    }

    void DisplayDemo::NextPattern()
    {
        if (display == nullptr)
            return;

        pattern = static_cast<Pattern>((static_cast<uint8_t>(pattern) + 1) % static_cast<uint8_t>(Pattern::count));

        if (drawing)
            restart = true;
        else
            Draw();
    }

    void DisplayDemo::Draw()
    {
        drawing = true;
        restart = false;
        nextRow = 0;
        BuildStrip();
        DrawNextStrip();
    }

    void DisplayDemo::BuildStrip()
    {
        for (uint16_t x = 0; x < width; ++x)
        {
            const uint16_t pixel = PixelAt(x);

            for (uint16_t row = 0; row < stripHeight; ++row)
            {
                const std::size_t index = (static_cast<std::size_t>(row) * width + x) * 2;
                strip[index] = static_cast<uint8_t>(pixel >> 8);
                strip[index + 1] = static_cast<uint8_t>(pixel & 0xff);
            }
        }
    }

    uint16_t DisplayDemo::PixelAt(uint16_t x) const
    {
        switch (pattern)
        {
            case Pattern::colorBars:
                return colorBars[x * colorBars.size() / width];
            case Pattern::greyRamp:
                return Grey(static_cast<uint8_t>(x * 255 / (width - 1)));
            case Pattern::red:
                return 0xf800;
            case Pattern::green:
                return 0x07e0;
            case Pattern::blue:
                return 0x001f;
            case Pattern::count:
                break;
        }

        return 0;
    }

    void DisplayDemo::DrawNextStrip()
    {
        display->Write({ 0, nextRow, width, stripHeight }, infra::MakeConstByteRange(strip), [this]()
            {
                StripDone();
            });
    }

    void DisplayDemo::StripDone()
    {
        nextRow += stripHeight;

        if (nextRow < height)
        {
            DrawNextStrip();
            return;
        }

        drawing = false;
        tracer.Trace() << "Pattern drawn: " << PatternName();

        if (restart)
            Draw();
    }

    const char* DisplayDemo::PatternName() const
    {
        switch (pattern)
        {
            case Pattern::colorBars:
                return "color bars";
            case Pattern::greyRamp:
                return "grey ramp";
            case Pattern::red:
                return "red";
            case Pattern::green:
                return "green";
            case Pattern::blue:
                return "blue";
            case Pattern::count:
                break;
        }

        return "";
    }
}
