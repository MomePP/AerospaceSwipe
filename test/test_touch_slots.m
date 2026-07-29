// Standalone unit test for the per-finger slot allocator in event_tap.m.
// Builds independently of the full app (no CGEventTap, no NSApplication) so
// it runs without Accessibility permission or a live trackpad. Run via
// `make test`.
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <assert.h>
#import "../src/event_tap.h"

// Regression: slots must be reclaimed when a contact ends, or a second
// trackpad exhausts the pool. Each multitouch device carries its own set of
// NSTouch identity objects, and a Bluetooth trackpad hands out a fresh set
// every time it re-enumerates (plug-in, sleep/wake). Identities are never
// equal across those generations, so without a reclaim on contact end the 16
// slots are consumed for good — touch_slot_acquire() then returns -1 for
// every finger, gesture displacement stops accumulating, and no swipe fires
// again until the process is restarted.
static void test_slots_survive_repeated_device_generations(void)
{
	for (int generation = 0; generation < 8; ++generation) {
		NSMutableArray* pool = [NSMutableArray array];

		for (int finger = 0; finger < 5; ++finger) {
			NSObject* identity = [NSObject new];
			[pool addObject:identity];

			int slot = touch_slot_acquire((__bridge const void*)identity);
			assert(slot >= 0 && slot < MAX_TOUCHES);
		}

		// Every contact lifts — the whole generation goes away with it.
		for (NSObject* identity in pool)
			touch_end((__bridge const void*)identity);
	}
}

int main(void)
{
	@autoreleasepool {
		// Runs first: the assertions below deliberately exhaust the pool.
		test_slots_survive_repeated_device_generations();

		// The two NSTouch phases that mean "this contact is over". Guarding
		// the literals here keeps the process_touches() reclaim path honest.
		assert(END_PHASE == NSTouchPhaseEnded);
		assert(CANCEL_PHASE == NSTouchPhaseCancelled);

		NSObject* fingerA = [NSObject new];
		NSObject* fingerB = [NSObject new];
		NSObject* fingerC = [NSObject new];

		int slotA = touch_slot_acquire((__bridge const void*)fingerA);
		assert(slotA >= 0 && slotA < MAX_TOUCHES);

		// Acquiring again for the SAME identity returns the SAME slot —
		// this is what keeps prev_x[slot]/base_x[slot] stable across
		// frames even though NSSet enumeration order is not.
		int slotAAgain = touch_slot_acquire((__bridge const void*)fingerA);
		assert(slotAAgain == slotA);

		// A different identity gets a different slot.
		int slotB = touch_slot_acquire((__bridge const void*)fingerB);
		assert(slotB >= 0 && slotB < MAX_TOUCHES);
		assert(slotB != slotA);

		// Releasing a slot frees it for reuse by a later identity.
		touch_slot_release((__bridge const void*)fingerA);
		int slotC = touch_slot_acquire((__bridge const void*)fingerC);
		assert(slotC == slotA);

		// Releasing an identity that was never acquired is a safe no-op.
		NSObject* neverAcquired = [NSObject new];
		touch_slot_release((__bridge const void*)neverAcquired);

		// Releasing the same identity twice is a safe no-op the second time.
		touch_slot_release((__bridge const void*)fingerB);
		touch_slot_release((__bridge const void*)fingerB);

		// Only slotC (reused from A) remains in use; MAX_TOUCHES - 1 slots
		// are free. Fill them all, then confirm the next acquire reports
		// exhaustion (-1) instead of corrupting state or crashing.
		NSMutableArray* fillers = [NSMutableArray array];
		for (int i = 0; i < MAX_TOUCHES - 1; ++i) {
			NSObject* filler = [NSObject new];
			[fillers addObject:filler];
			int slot = touch_slot_acquire((__bridge const void*)filler);
			assert(slot >= 0 && slot < MAX_TOUCHES);
		}

		NSObject* oneTooMany = [NSObject new];
		int overflowSlot = touch_slot_acquire((__bridge const void*)oneTooMany);
		assert(overflowSlot == -1);
	}

	printf("test_touch_slots: all assertions passed\n");
	return 0;
}
