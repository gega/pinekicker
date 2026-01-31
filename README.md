# pinekicker

**pinekicker** is a minimal, fast **XIP A/B bootloader** for ARM Cortex-M4 microcontrollers.

The primary target is **PineTime (Nordic nRF52)**, but the design is generic and should work on other Nordic Cortex-M platforms with minimal effort.

The focus is on **fast boot**, **simple integration**, and **strong firmware authenticity guarantees**, without unnecessary features or peripherals.

---

## Features

* **A/B slot booting with rollback**

  * Two statically defined firmware slots
  * Automatic fallback if the new image is invalid

* **Version-based selection**

  * Monotonic version comparison
  * Version is trusted only after signature verification

* **Secure firmware authentication**

  * ECDSA P-256 (prime256v1)
  * SHA-256 over the entire firmware image
  * Public key is compiled into the bootloader
  * Signature stored in the slot header

* **Fast, deterministic boot**

  * Fixed slot layout
  * Minimal parsing before cryptographic verification

* **XIP design**

  * Firmware executes directly from flash
  * No slot swapping or image copying

* **Supported cores**

  * ARM Cortex-M4
  * Tested primarily on Nordic nRF52

---

## Application Requirements

Each application image **must contain a slot header** located **within the first 4 KB** of the image.

The header **must be a packed struct with fixed offsets**.

### Slot Header Layout

```c
struct slot_header {
    uint32_t magic;         // SLOT_MAGIC
    uint32_t version;
    uint32_t vtor_offset;
    uint32_t status;        // SLOT_STATUS_NEW
    uint32_t length;        // filled post-build
    uint32_t load_addr;     // filled post-build
    uint8_t  signature[64]; // filled post-build
} __attribute__((packed));
```

Field offsets are fixed and must not change.

### Initialization

The application **must initialize**:

* `magic`
* `version`
* `vtor_offset`
* `status` (set to `SLOT_STATUS_NEW`)

The following fields **must be left as `0xFF`** and are filled by tooling:

* `length`
* `load_addr`
* `signature`

Notes:

* The header does **not** need to be at a fixed offset, only within the first 4 KB.
* `vtor_offset` is relative to the slot base address.

### Linker Support

For reproducible builds, defining ```__image_load_addr``` is preferred over relying on ELF program headers.
```
__image_load_addr = ADDR(.text);
```

---

## Tooling

### `genkey.sh`

Generates an ECC key pair for firmware signing.

```
genkey.sh <name> <header_name>
```

* Uses ECDSA prime256v1
* Requires `openssl`
* Outputs:

  * Private key (`.pem`)
  * C header with raw public key bytes

---

### `postbuild.sh`

Generates a signed slot binary from an ELF file.

```
postbuild.sh <input.elf> <output_basename> <signing_key.pem>
```

What it does:

* Extracts a raw binary from ELF
* Locates the slot header within the first 4 KB
* Computes SHA-256 over the full image
* Signs the hash using ECDSA
* Fills `length`, `load_addr`, and `signature`
* Produces a slot binary named with its slot address

---

### `prepflash.sh`

End-to-end bootloader test using QEMU.

```
prepflash.sh <bootloader.bin> <slot1.bin> <slot2.bin>
```

* Builds a synthetic flash image
* Boots the bootloader under QEMU
* Injects breakpoints at application reset handlers
* Reports which slot was selected
* Times out after 5 seconds on failure

User code is never executed.

---

## Unit Tests

The boot decision logic is covered by a **table-driven unit test framework** that runs entirely on the host.

The test binary compiles the real bootloader logic directly and replaces hardware-specific behavior with test hooks.

### `pinekicker_test`

The unit test validates:

* Slot selection logic
* One way status transitions (`NEW`, `TESTING`, `CONFIRMED`, `FAILED`)
* Version comparison rules
* Number of flash writes performed
* Slot header discovery within the scan limit
* Signature verification logic (optional)

The test matrix explicitly enumerates all meaningful combinations of:

* Slot A/B status
* Slot A/B version
* Expected selected slot
* Expected number of status updates
* Whether a valid slot was found

This ensures deterministic behavior for every supported state transition.

---

### Running the tests

Build and run the decision logic tests:

```
gcc -o pinekicker_test -DUNIT_TEST src/pinekicker_test.c
./pinekicker_test
```

This runs the full slot-selection test matrix and reports failures immediately.

---

### Signature verification tests

`pinekicker_test` can also validate real, signed slot images produced by `postbuild.sh`.

```
./pinekicker_test 1fw_good_0x8000.bin 0fw_bad_0x9000.bin
```

Argument format:

* First character: expected verification result

  * `1` = signature must be valid
  * `0` = signature must be invalid
* Remaining characters: filename of the slot image

Any number of images may be specified.

This allows end-to-end verification of:

* Hashing
* Signature format
* Public key usage
* Header parsing

---

## Dependencies

Included in the repository:

* SHA-256 implementation
  [https://lucidar.me/en/dev-c-cpp/sha-256-in-c-cpp/](https://lucidar.me/en/dev-c-cpp/sha-256-in-c-cpp/)
* micro-ECC (uECC) by Kenneth MacKay

Not included:

* Nordic nRF platform headers

Toolchain requirements:

* `arm-none-eabi-*`
* `objcopy`, `readelf`, `gdb`
* `qemu-system-arm`
* `openssl`

---

## Design Notes

This is an **XIP bootloader** by design.

The primary goal is to **boot quickly and predictably**.

Design choices:

* No slot swapping
* No image copying
* No filesystem
* No unnecessary peripherals
* No boot graphics
* **Deterministic behavior**

  * Every slot state combination has a defined outcome
* **Minimal flash writes**

  * Status updates are strictly bounded and verified
* **No implicit recovery logic**

  * All transitions are explicit and test-covered

The concept is simple: a bootloader should make a decision and get out of the way.
