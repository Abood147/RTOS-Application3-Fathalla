# RTOS-Application3-Fathalla
## Engineering Analysis

### 1. Polling vs. Interrupt

ISR + semaphore is more efficient because the CPU does zero work waiting for the button. With polling, the task runs every few milliseconds checking a pin regardless of whether anything happened, that's wasted cycles and adds arbitrary latency depending on the poll interval. With the ISR approach, the CPU is free to do other work or sleep, and the response is immediate the moment the button is pressed.

### 2. ISR Design

FromISR variants must be used inside an ISR because regular FreeRTOS API calls can block, and blocking inside an ISR is a deadlock. The ISR freezes the scheduler context, so nothing else can run including whatever task would eventually unblock it. For example, calling `xSemaphoreTake` inside an ISR would wait forever since no other task can run to give it. The FromISR variants are specifically designed to be non-blocking and interrupt-safe.

### 3. Real-Time Behavior

When the button is pressed while the sensor task is running, the ISR fires immediately regardless of what's executing. Inside the ISR, `xSemaphoreGiveFromISR` unblocks the logger task, and `portYIELD_FROM_ISR` signals the scheduler to switch context right away if a higher priority task is now ready. Since the logger is priority 3 and the sensor is priority 2, the logger preempts immediately, it doesn't wait for the sensor task to finish its current cycle.

### 4. Core Affinity

Without pinning to Core 1, tasks could migrate between cores unpredictably. This makes scheduling behavior nondeterministic since two tasks could run simultaneously on different cores, causing race conditions on shared resources like the sensor buffer without proper protection. Pinning everything to Core 1 keeps scheduling predictable and makes it easier to reason about task interactions and timing.

### 5. Light Sensor Logging

The sensor buffer is shared between the sensor and logger tasks, so access is protected with `xLogMutex`. The sensor task locks the mutex before writing a new reading, and the logger locks it before copying the buffer. If the logger preempted mid-write without the mutex, it could read a partially updated buffer. For example catching a new index value but an old reading, producing incorrect stats.

### 6. Task Priorities

If the logger was priority 1 and blink was priority 3, pressing the button would give the semaphore but the logger would never preempt the blink task since blink has higher priority. The logger would only run when both the blink and sensor tasks are blocked. This demonstrates how priority inversion breaks real time responsiveness, a critical event handler gets starved by a less important task simply because priorities are assigned incorrectly.

### 7. Resource Usage

Two reasons to keep ISR work minimal: first, ISRs block all same-priority interrupts while running, so a long ISR delays other time-sensitive events. Second, most FreeRTOS APIs aren't safe to call from an ISR context. So doing heavy work there risks calling something blocking by accident. In this lab the ISR only gives a semaphore and yields, leaving all processing to the logger task where it's safe.

### 8. Chapter Connections

Section 7.3 of Mastering the FreeRTOS Kernel describes deferred interrupt processing — the idea that "any other processing necessitated by the interrupt can often be performed in a task, allowing the interrupt service routine to exit as quickly as is practical." Section 7.4 then describes how a binary semaphore achieves this: the deferred processing task blocks on `xSemaphoreTake()`, and the ISR uses `xSemaphoreGiveFromISR()` to unblock it when the event occurs. This is exactly the pattern used in this application, the button ISR does nothing except give the semaphore, and the logger task handles all the actual processing once it wakes up.