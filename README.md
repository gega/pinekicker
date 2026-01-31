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

### Initialization Rules

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

The motivation is simple: a bootloader should make a decision and get out of the way.
