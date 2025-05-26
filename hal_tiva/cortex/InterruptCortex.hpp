#ifndef HAL_INTERRUPT_CORTEX_HPP
#define HAL_INTERRUPT_CORTEX_HPP

#include DEVICE_HEADER
#include "infra/util/Function.hpp"
#include "infra/util/InterfaceConnector.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <atomic>

namespace hal
{
    enum class InterruptPriority : uint8_t
    {
        // Critical system priorities
        Highest = 0,  // Highest priority, use sparingly for critical real-time tasks
        VeryHigh = 1, // Very high priority for urgent time-critical tasks
        High = 2,     // High priority for important but not critical tasks

        // Normal application priorities
        AboveNormal = 3, // Above normal priority
        Normal = 4,      // Default priority for most interrupts
        BelowNormal = 5, // Below normal priority

        // Background task priorities
        Low = 6,     // Low priority for non-urgent interrupts
        VeryLow = 7, // Very low priority for background processing
        Lowest = 15, // Lowest possible priority

        // Aliases for backward compatibility
        Default = Normal,
        Background = Low
    };

    IRQn_Type ActiveInterrupt();

    class InterruptHandler
    {
    protected:
        InterruptHandler();
        InterruptHandler(const InterruptHandler& other) = delete;
        InterruptHandler(InterruptHandler&& other);
        InterruptHandler& operator=(const InterruptHandler& other) = delete;
        InterruptHandler& operator=(InterruptHandler&& other);
        ~InterruptHandler();

    public:
        void Register(IRQn_Type irq, InterruptPriority priority = InterruptPriority::Normal);
        void Unregister();
        virtual void Invoke() = 0;

        IRQn_Type Irq() const;

        void ClearPending();

    private:
        IRQn_Type irq;
    };

    class InterruptTable
        : public infra::InterfaceConnector<InterruptTable>
    {
    public:
        template<std::size_t Size>
        using WithStorage = infra::WithStorage<InterruptTable, std::array<InterruptHandler*, Size>>;

        InterruptTable(infra::MemoryRange<InterruptHandler*> table);

        void Invoke(IRQn_Type irq);
        InterruptHandler* Handler(IRQn_Type irq);

    private:
        friend class InterruptHandler;

        void RegisterHandler(IRQn_Type irq, InterruptHandler& handler, InterruptPriority priority);
        void DeregisterHandler(IRQn_Type irq, InterruptHandler& handler);
        void TakeOverHandler(IRQn_Type irq, InterruptHandler& handler, const InterruptHandler& previous);

    private:
        infra::MemoryRange<InterruptHandler*> table;
    };

    class DispatchedInterruptHandler
        : public InterruptHandler
    {
    public:
        DispatchedInterruptHandler(IRQn_Type irq, const infra::Function<void()>& onInvoke);
        DispatchedInterruptHandler(const DispatchedInterruptHandler& other) = delete;
        DispatchedInterruptHandler(DispatchedInterruptHandler&& other) = delete;
        DispatchedInterruptHandler(DispatchedInterruptHandler&& other, const infra::Function<void()>& onInvoke);
        DispatchedInterruptHandler& operator=(const DispatchedInterruptHandler& other) = delete;
        DispatchedInterruptHandler& operator=(DispatchedInterruptHandler&& other) = delete;
        DispatchedInterruptHandler& Assign(DispatchedInterruptHandler&& other, const infra::Function<void()>& onInvoke);

        virtual void Invoke() final;
        void SetInvoke(const infra::Function<void()>& onInvoke);

    private:
        static void InvokeScheduled(IRQn_Type irq, DispatchedInterruptHandler& handler);

    private:
        infra::Function<void()> onInvoke;
        bool pending = false;
    };

    class ImmediateInterruptHandler
        : public InterruptHandler
    {
    public:
        ImmediateInterruptHandler(IRQn_Type irq, const infra::Function<void()>& onInvoke);
        ImmediateInterruptHandler(const ImmediateInterruptHandler& other) = delete;
        ImmediateInterruptHandler(ImmediateInterruptHandler&& other) = delete;
        ImmediateInterruptHandler(ImmediateInterruptHandler&& other, const infra::Function<void()>& onInvoke);
        ImmediateInterruptHandler& operator=(const ImmediateInterruptHandler& other) = delete;
        ImmediateInterruptHandler& operator=(ImmediateInterruptHandler&& other) = delete;
        ImmediateInterruptHandler& Assign(ImmediateInterruptHandler&& other, const infra::Function<void()>& onInvoke);

        virtual void Invoke() final;

    private:
        infra::Function<void()> onInvoke;
        // Indicates whether an interrupt is currently being processed.
        // Used to prevent reentrant interrupt handling by ensuring that
        // an interrupt is not processed again while it is already being handled.
        std::atomic<bool> processingInterrupt{ false };
    };
}

#endif
