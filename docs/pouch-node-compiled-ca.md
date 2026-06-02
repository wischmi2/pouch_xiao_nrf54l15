# Pouch Node Certificates: `0x2700, 8` (server cert) and `4.12` (device cert)

This note covers the two certificates a XIAO nRF54L15 Pouch node depends on, and
how to diagnose and fix the two failures they cause.

```text
# Failure A - node rejects the server cert (compiled-in CA problem)
<err> cert: Failed verifying server cert: 0x2700, 8
<err> saead_session: Missing server key
<err> saead_uplink: Session key generation failed

# Failure B - Golioth rejects the node's device cert (expired cert / CA)
<inf> cert: Device cert cloud set CoAP response: 4.12
<err> cert: Failed to set cert: 17
<err> os: ***** HARD FAULT *****   (gateway crashes/reboot-loops)
```

> **Quick decision guide.** A fully working node needs **both** halves right:
> 1. **Good firmware** with the correct compiled-in CA, so the node trusts the
>    server cert (no `0x2700, 8`) — see "Failure A" below.
> 2. A **valid, Golioth-registered device cert** in LittleFS, so Golioth accepts
>    the node identity (`2.05`, not `4.12`) — see the section
>    **"Failure B: expired device cert"** below.
>
> In the field, the most common cause turned out to be **Failure B**: a
> short-lived demo device cert (and its CA) expired, which looks like a re-flash
> regression but is really a calendar problem. See the
> **"Known-good healthy sync reference"** section for exactly what a working
> system logs.

## What the error means

The error is logged by the node in `pouch/src/cert.c`
(`authenticate_server_cert()`):

- `0x2700` = mbedTLS `MBEDTLS_ERR_X509_CERT_VERIFY_FAILED`.
- `8` = the verify flag `MBEDTLS_X509_BADCERT_NOT_TRUSTED`, i.e. *"the
  presented certificate is not signed by a CA I trust."*

The node received the Golioth **server certificate chain** (the gateway
downloads it from Golioth and relays it over GATT) and rejected it because the
chain does not terminate at the **CA trust anchor compiled into the node
firmware**.

This is a runtime TLS trust failure. It is **not** a programming, J-Link, or
nRF54L15 readback-protection (`--recover`) error.

## The two certificates

The node uses two different kinds of certificates. Do not confuse them:

| Cert | Purpose | Where it lives | Set when |
|---|---|---|---|
| **Server CA trust anchor** (`goliothrootx1.der`) | verifies the server cert the gateway sends | compiled into the firmware image | **build time** |
| **Device identity** (`crt.der` / `key.der`) | identifies the node to Golioth | LittleFS `/lfs1/credentials/` | runtime upload |

The cert that produces `0x2700, 8` is the **compiled-in CA**. The device identity
files are uploaded at runtime:

```text
C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.crt.der
C:/ncs_pouch_soil/pouch/certs/chocolate-voiceless-mastodon.key.der
```

## Why a re-flash / rebuild can trigger it

Because the trust anchor is fixed at **build time** but the server cert arrives
at **runtime**, "it worked, then I rebuilt/reflashed and it broke" usually means
the compiled bytes changed, the node config changed, or the environment the
gateway talks to changed.

The two most common causes:

1. **Wrong/corrupt compiled CA file.** `pouch/src/goliothrootx1.der` was
   overwritten or replaced (e.g. with a dev root or with one of the device
   certs). Every build from that tree — including a rebuilt, previously-working
   branch — then bakes in a CA that no longer matches Golioth's server cert.
   (If the file were simply missing, the build would fail via
   `find_file(... REQUIRED)`, so a silent runtime break means "present but
   wrong contents.")
2. **Prod/dev environment mismatch** between the gateway and the node.

> A wiped LittleFS produces `Failed to load certificate (err -2)` (the
> device-identity cert), **not** `0x2700, 8`. Different failure — see step 5
> below to re-upload the device cert/key.

## How prod vs. dev is selected

**Gateway (FRDM-RW612):** `CONFIG_GOLIOTH_COAP_HOST_URI`
(sysbuild `SB_CONFIG_GOLIOTH_COAP_HOST_URI`) selects the endpoint:

- `coaps://coap.golioth.io` = **production** (default)
- `coaps://coap.golioth.dev` = development

The gateway downloads the server cert chain from that endpoint (with
`CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN` not set) and relays it to the node.
The runtime PSK (`golioth/psk-id`, `golioth/psk`) decides which Golioth
project/instance actually serves that cert.

**Node (XIAO nRF54L15):** trusts only what it compiled in:

- CA: `CONFIG_POUCH_CA_CERT_FILENAME` (default `src/goliothrootx1.der`, the
  production Golioth Root X1).
- Expected hostname: `CONFIG_POUCH_SERVER_CERT_CN` (default `pouch.golioth.io`).

The node CN is auto-switched to `pouch.golioth.dev` **only in the BabbleSim
(`bsim`) build** (see `examples/gateway/sysbuild.cmake`). A real-hardware node
keeps `pouch.golioth.io` + Root X1 unless changed by hand. So if the gateway is
talking to dev while the node was built for prod (or vice versa), verification
fails with `0x2700, 8`.

## Diagnosing the failure

### Step 1 - Capture the node serial log

The node prints both the compiled trust anchor and the received server cert
right before failing. Connect to the XIAO serial console (COM8 on this setup),
then reboot both boards:

```text
kernel reboot
```

Look for, in order:

```text
<inf> cert: Loaded Pouch CA cert (N bytes)
<inf> cert: Pouch CA cert:
  ! ... subject / issuer of the COMPILED-IN trust anchor ...
<inf> cert: Received server cert chain (M bytes)
<inf> cert: Received server cert chain:
  ! ... subject / issuer / validity of what the GATEWAY delivered ...
<err> cert: Failed verifying server cert: 0x2700, 8
<err> cert: Server cert verify flags:
  ! The certificate is not correctly signed by the trusted CA
```

### Step 2 - Compare the two certs (the actual test)

- **Received server cert subject/CN:** production is `pouch.golioth.io`. If it
  shows `pouch.golioth.dev` (or anything else), the gateway is on a different
  environment -> **mismatch confirmed**.
- **Received server cert issuer vs. compiled CA subject:** if the received
  chain's root issuer is not the same Golioth root you compiled in, that is
  exactly what flag `8` (NOT_TRUSTED) reports -> **mismatch (or wrong CA file)
  confirmed**.
- If the CN matches `pouch.golioth.io` **and** the issuer chain matches your
  compiled root, the environment mismatch is **ruled out** — investigate the
  compiled CA bytes (cause #1) instead.

> Interpretation caveat: a *pure* CN-only mismatch (right root, wrong hostname)
> sets flag `4` (`CN_MISMATCH`). A different signing root sets flag `8`
> (`NOT_TRUSTED`). Since the symptom is `8`, the strongest signal is the
> **issuer comparison**, not just the hostname.

### Step 3 - Confirm the node firmware's expectation

```powershell
cd C:\ncs_pouch_soil\pouch
Get-ChildItem -Recurse -Filter .config examples\ble_gatt | Select-Object FullName
Get-Content examples\ble_gatt\build\zephyr\.config |
  Select-String "POUCH_SERVER_CERT_CN|POUCH_CA_CERT_FILENAME|POUCH_VALIDATE_SERVER_CERT"
```

For production, expect:

```text
CONFIG_POUCH_SERVER_CERT_CN="pouch.golioth.io"
CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"
CONFIG_POUCH_VALIDATE_SERVER_CERT=y
```

If a branch flipped the CN to `pouch.golioth.dev`, that alone causes a mismatch.

### Step 4 - Confirm what environment the gateway actually uses

Build-time endpoint:

```powershell
Get-Content C:\ncs_pouch_soil\build-gateway-frdm_rw612\zephyr\.config |
  Select-String "GOLIOTH_COAP_HOST_URI|POUCH_GATEWAY_SERVER_CERT_BUILTIN"
```

Expected for prod:

```text
CONFIG_GOLIOTH_COAP_HOST_URI="coaps://coap.golioth.io"
# CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN is not set
```

Runtime project/instance — on the **gateway** serial shell:

```text
settings get golioth/psk-id
```

The id is `deviceId@projectId`. Confirm `projectId` is the project the node's
device cert was provisioned into, on the **production** console
(`console.golioth.io`). Also watch the gateway boot log for the host it connects
to and `Golioth client connected`.

## Known-good reference: the compiled CA

For comparison against the node's `Pouch CA cert` log line and the `Received
server cert chain` issuer, the production trust anchor currently compiled into
the node (`pouch/src/goliothrootx1.der`, 548 bytes) decodes to:

| Field | Value |
|---|---|
| Subject | `CN=Golioth Root X1, O="Golioth, Inc.", C=US` |
| Issuer | `CN=Golioth Root X1, O="Golioth, Inc.", C=US` (self-signed root) |
| Serial | `00EE8FA620F7C94862797617A8943FF43BE7287D` |
| Valid from | `2024-11-19 10:52:34Z` |
| Valid to | `2044-11-14 10:52:33Z` |
| Signature alg | `sha384ECDSA` |
| Public key | ECC (secp384r1) |
| Basic Constraints | `Subject Type=CA` (CA:TRUE) |
| Key Usage | Certificate Signing, CRL Signing |
| SHA-1 thumbprint | `D453A7524E65C1C021AE63B925208477573ECE81` |
| SHA-256 (DER) | `F0:3F:EC:5A:25:B8:D5:DD:A4:55:24:C8:71:A7:59:13:EF:55:FB:B6:94:08:E8:A2:DD:CC:93:5F:6F:CA:98:A0` |

A correctly verifying setup requires the **issuer** of the received server cert
chain to be exactly `CN=Golioth Root X1, O="Golioth, Inc.", C=US` and the leaf
CN to be `pouch.golioth.io`. Anything else points to a dev/prod (or wrong-CA)
mismatch.

To re-derive this on Windows (no OpenSSL needed):

```powershell
$c = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2('C:\ncs_pouch_soil\pouch\src\goliothrootx1.der')
"Subject: $($c.Subject)"; "Issuer: $($c.Issuer)"; "Serial: $($c.SerialNumber)"
"NotBefore: $($c.NotBefore.ToString('u'))"; "NotAfter: $($c.NotAfter.ToString('u'))"
$sha=[System.Security.Cryptography.SHA256]::Create()
"SHA256-DER: " + ((($sha.ComputeHash($c.RawData))|ForEach-Object {$_.ToString('X2')}) -join ':')
```

## Fixing it

### 1. Place the correct CA file

For the production gateway that connects to `coap.golioth.io`, the node needs the
production Golioth Root X1 CA certificate in DER format at the default path:

```text
C:/ncs_pouch_soil/pouch/src/goliothrootx1.der
```

The relevant Kconfig default is:

```text
CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"
```

During the Zephyr build, `port/zephyr/CMakeLists.txt` finds that file, generates
`pouch_ca_cert.inc`, and links the bytes into the node firmware.

### 2. Rebuild the XIAO node firmware

From PowerShell:

```powershell
cd C:/ncs_pouch_soil/pouch
west config manifest.file west-ncs.yml
west update
powershell -ExecutionPolicy Bypass -File scripts/build_ble_gatt_v010.ps1 -SkipWestUpdate
```

The build helper builds `examples/ble_gatt` for `xiao_nrf54l15/nrf54l15/cpuapp`
using `--no-sysbuild`, which is the current working path for this board.

The current stable soil sensor node image also uses:

```text
CONFIG_POUCH_THREAD_STACK_SIZE=8192
CONFIG_POUCH_UPLINK_PROCESSING_STACK_SIZE=8192
CONFIG_GOLIOTH_SETTINGS=n
CONFIG_GOLIOTH_OTA=n
```

The larger Pouch stacks prevent the callback/uplink path from corrupting the node
during soil sensor sync. `CONFIG_GOLIOTH_SETTINGS=y` currently reproduces a
`pouch_work` crash after server certificate verification, so cloud Settings are
disabled for the working soil sensor build.

### 3. Flash the XIAO node

With the XIAO debugger connected:

```powershell
pyocd flash -t nrf54l C:/ncs_pouch_soil/pouch/examples/ble_gatt/build/zephyr/zephyr.hex
pyocd reset -t nrf54l
```

After reboot, the node should show:

```text
<inf> main: Credentials loaded
<inf> main: Pouch initialized
<inf> main: Advertising started
```

The XIAO LED reports startup progress with blink groups:

```text
1 blink  - application started and LED GPIO initialized
2 blinks - Bluetooth initialized
3 blinks - credentials loaded and Pouch initialized
4 blinks - button and soil sensor setup completed
5 blinks - BLE advertising started and gateway sync requested
```

If the LED stops before five blinks, check the serial log for the failing stage.

### 4. Re-upload the node device cert and key if needed

Flashing the app normally should not erase LittleFS, but if the filesystem was
erased or reformatted, upload the node cert and key again. Close the COM8 serial
terminal first, then run from PowerShell:

```powershell
cd C:/ncs_pouch_soil/pouch
$env:PYTHONUTF8='1'
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file upload .\certs\chocolate-voiceless-mastodon.crt.der /lfs1/credentials/crt.der
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file upload .\certs\chocolate-voiceless-mastodon.key.der /lfs1/credentials/key.der
```

To check whether they are present:

```powershell
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file read-size /lfs1/credentials/crt.der
smpmgr.exe --port COM8 --line-length 128 --line-buffers 2 file read-size /lfs1/credentials/key.der
```

Expected sizes are `380` bytes for `crt.der` and `138` bytes for `key.der`. If
LittleFS reformats after flashing, the node stops at:

```text
<err> main: Failed to load certificate (err -2)
```

Re-upload the cert and key, then reset the node.

### 5. Make both ends agree on one environment

If step 2 confirmed a prod/dev mismatch:

- **Production (matches the current gateway build):** rebuild the node from a
  clean build dir with `CONFIG_POUCH_SERVER_CERT_CN="pouch.golioth.io"` and
  `CONFIG_POUCH_CA_CERT_FILENAME="src/goliothrootx1.der"`, and ensure the gateway
  PSK belongs to a production-project device.
- **Development:** point the gateway at `coaps://coap.golioth.dev` **and** rebuild
  the node with `CONFIG_POUCH_SERVER_CERT_CN="pouch.golioth.dev"` plus the
  matching dev root CA `.der` in `CONFIG_POUCH_CA_CERT_FILENAME`.

### 6. Retry the gateway sync

Reboot both boards:

```text
kernel reboot
```

Expected node behavior after the gateway connects:

```text
<inf> main: BT security changed to level 2
```

The node should no longer print `Failed verifying server cert: 0x2700, 8` or
`Missing server key`.

The known-working production path is to let the gateway download the server
certificate chain from Golioth at runtime and send that chain to the node, so the
FRDM-RW612 WiFi gateway build keeps:

```text
CONFIG_POUCH_GATEWAY_SERVER_CERT_BUILTIN=n
```

Do not force the bundled gateway server certificate for this setup. That can make
the node verify the certificate successfully while still encrypting the Pouch
uplink to a public key that Golioth does not accept. The gateway symptom is:

```text
<err> uplink: Uplink CoAP response: 4.00
```

The expected successful gateway logs are:

```text
<inf> cert: Device cert cloud set CoAP response: 2.05
<inf> cert: Golioth accepted node device cert
<inf> uplink: Sending uplink block 0: len 111, last 1, closed 1
<inf> uplink: Delivered uplink block 0: path pouch, block_size 1024
```

## Troubleshooting

If `0x2700, 8` continues, rebuild from a pristine build directory and confirm the
CA file is present before building:

```powershell
Test-Path C:/ncs_pouch_soil/pouch/src/goliothrootx1.der
powershell -ExecutionPolicy Bypass -File scripts/build_ble_gatt_v010.ps1 -SkipWestUpdate
```

If the gateway is changed to a non-production Golioth endpoint, such as a dev
endpoint, the node must be built with the matching CA and expected server common
name. The production defaults are for `coap.golioth.io` and `pouch.golioth.io`.

# Failure B: expired device cert -> gateway `4.12` -> crash

Everything above is "Failure A" (the node distrusts the **server** cert). There is
a second, independent failure on the **device-identity** cert.

The node stores its own identity cert/key in LittleFS
(`/lfs1/credentials/crt.der`, `key.der`). The gateway reads that cert over BLE and
registers it with Golioth. If the device cert (or the CA that signed it) has
**expired** or is **not in the Golioth project**, Golioth rejects it:

```text
<inf> cert: Finishing device cert from node: len 380
<inf> cert: Sending node device cert to Golioth gateway API
<inf> cert: Device cert cloud set CoAP response: 4.12     # 4.12 = Precondition Failed
<err> cert: Failed to set cert: 17
<err> device_cert_gatt: Failed to finish device cert: -5
<err> os: ***** HARD FAULT *****
<err> os:   Bus fault on vector table read
<err> os: Current thread: 0x... (coap_client)
<err> fatal_error: Resetting system
```

Two important facts learned in the field:

- **It looks like a re-flash regression but it is calendar-driven.** The demo
  device certs and their CAs were issued with ~28-day validity. When they lapsed
  (e.g. `chocolate-voiceless-mastodon` expired 2026-05-22, its
  `chocolate-voiceless-mastodon-CA` also expired), every sync started returning
  `4.12`. Re-flashing around the same time made it *look* causal.
- **The gateway hard-faults on the rejection.** The `4.12` itself is a clean
  cloud error, but the gateway firmware then bus-faults in the `coap_client`
  thread and reboot-loops. This is a latent gateway bug (see the last section);
  a valid cert avoids triggering it but does not fix it.

> `4.12` is the **device** cert (LittleFS, Golioth side). `0x2700, 8` is the
> **server** cert (compiled-in CA, node side). They are unrelated; a node can
> pass one and fail the other.

## You need both halves: firmware AND a valid device cert

Real units observed (one Golioth project, `emerald-mature-damselfly`):

| Node name | Firmware (server cert) | Device cert | Result |
|---|---|---|---|
| `chocolate-voiceless-mastodon` | good (verifies) | expired | `4.12` -> gateway crash |
| `xiao-nrf54l15-two` | good (verifies) | expired | `4.12` -> gateway crash |
| `xiao-nrf54l15-three` | bad (`0x2700, 8`) | valid | server cert never trusted |
| `xiao-nrf54l15-four` | good (verifies) | **valid** | **works** |

A working node = **good firmware** (Failure A clear) **+** a **valid, registered
device cert** (Failure B clear).

## Reading and checking a node's device cert over serial

You can pull the device cert straight out of a running node's LittleFS over the
shell/MCUmgr transport and check its expiry. On Windows, set `PYTHONUTF8=1` first
or `smpmgr`'s progress spinner crashes with a `charmap`/`UnicodeEncodeError`.

```powershell
cd C:\ncs_pouch_soil\pouch
$env:PYTHONUTF8='1'
# size only (383 = three/four, 380 = two/xiao-pouch-device, 401 = chocolate)
smpmgr --port COM17 --line-length 128 --line-buffers 2 file read-size /lfs1/credentials/crt.der
# download and decode (no OpenSSL needed)
smpmgr --port COM17 --line-length 128 --line-buffers 2 file download /lfs1/credentials/crt.der C:\ncs_pouch_soil\node_crt.der
$c = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2('C:\ncs_pouch_soil\node_crt.der')
"Subject: $($c.Subject)"; "Issuer: $($c.Issuer)"
"Valid: $($c.NotBefore.ToString('u')) -> $($c.NotAfter.ToString('u'))"; "Expired: $((Get-Date) -gt $c.NotAfter)"
```

The subject encodes the Golioth identity: `CN=<device-name>, O=<project-id>`
(for example `CN=xiao-nrf54l15-four, O=emerald-mature-damselfly`).

## Fix B-1: provision a valid (already-issued) device cert

If you already have a valid device cert whose CA is uploaded and unexpired in the
Golioth project, just upload it to a **good-firmware** node and power-cycle:

```powershell
cd C:\ncs_pouch_soil\pouch
$env:PYTHONUTF8='1'
smpmgr --port COM17 --line-length 128 --line-buffers 2 file upload .\certs\xiao-nrf54l15-four\xiao-nrf54l15-four.crt.der /lfs1/credentials/crt.der
smpmgr --port COM17 --line-length 128 --line-buffers 2 file upload .\certs\xiao-nrf54l15-four\xiao-nrf54l15-four.key.der /lfs1/credentials/key.der
```

Then **power-cycle BOTH boards**. This matters: rebooting the node gives it a new
random BLE address, so the gateway has to re-pair, and the gateway may be in a
30-second security cooldown or a hung post-crash state. Power-cycling both clears
stale bonds and lets the gateway reconnect cleanly. (`kernel reboot` on just the
node is often not enough.)

## Fix B-2: generate a long-lived CA + device cert

The demo certs expire fast. To stop this recurring, generate a long-lived CA and
device cert (P-256/SHA-256, to match the node), upload the CA to your Golioth
project's **Offline PKI**, and provision the device cert with Fix B-1.

```bash
# fresh CA (10y)
openssl ecparam -name prime256v1 -genkey -noout -out ca-key.pem
openssl req -x509 -new -nodes -key ca-key.pem -sha256 -days 3650 \
  -subj "/C=US/CN=my-pouch-ca" -out ca-cert.pem

# device key + CSR. CN = Golioth device name, O = Golioth project id.
openssl ecparam -name prime256v1 -genkey -noout -out key.pem
openssl req -new -key key.pem \
  -subj "/C=US/O=emerald-mature-damselfly/CN=xiao-nrf54l15-four" -out dev.csr

# sign (e.g. 5y) and convert to the DER form the node expects
openssl x509 -req -in dev.csr -CA ca-cert.pem -CAkey ca-key.pem -CAcreateserial \
  -days 1825 -sha256 -out crt.pem
openssl x509 -in crt.pem -outform der -out crt.der
openssl ec -in key.pem -outform der -out key.der
```

Then:

1. Upload `ca-cert.pem` to the Golioth project (Offline PKI -> upload CA).
2. Upload `crt.der` / `key.der` to the node with Fix B-1.
3. Power-cycle both boards.

> The node's compiled-in **server** CA (`goliothrootx1.der`) is unrelated and does
> not need regenerating - it is Golioth's long-lived root (valid to 2044).

# Known-good healthy sync reference

This is exactly what a fully working system logs (node `xiao-nrf54l15-four`, good
firmware `v0.1.0-221`, valid device cert, both boards power-cycled). Use it as the
baseline to compare against.

**Gateway (FRDM-RW612) - repeats cleanly every ~30s:**

```text
<inf> main: Connected: D2:B3:48:4F:7F:F7 (random)
<inf> main: Pairing complete for D2:B3:48:4F:7F:F7 (random), bonded 1
<inf> main: BT security changed for D2:B3:48:4F:7F:F7 (random) to level 2
<inf> server_cert_gatt: Server cert already provisioned, skipping write
<inf> cert: Finishing device cert from node: len 383
<inf> cert: Sending node device cert to Golioth gateway API
<inf> cert: Device cert cloud set callback: status 0, path device-cert
<inf> cert: Device cert cloud set CoAP response: 2.05
<inf> cert: Golioth accepted node device cert
<inf> uplink: Sending uplink block 0: len 139, last 1, closed 1
<inf> uplink: Delivered uplink block 0: path pouch, block_size 1024
<inf> main: Disconnected: D2:B3:48:4F:7F:F7 (random), reason 0x16
<inf> scan: Scanning successfully started
```

**Node (XIAO) - running its application:**

```text
<inf> main: Reading soil sensor ADC: device adc@d5000, channel 0, resolution 12
<inf> main: Soil sensor ADC sample: raw 0, millivolts 0
<inf> main: Writing soil sensor uplink: path .s/sensor, content_type 50, len 72
<inf> glth_dispatch: Receiving Downlink entry on path /.c
```

## Success markers vs. broken states

| Stage | Working | Broken |
|---|---|---|
| Server cert (node) | encrypted uplinks flow | `Failed verifying server cert: 0x2700, 8` |
| Device cert (gateway -> cloud) | `2.05` / `Golioth accepted node device cert` | `4.12` / `Failed to set cert: 17` |
| Gateway stability | clean `Disconnected reason 0x16`, keeps scanning | `HARD FAULT` -> `Resetting system` loop |
| Data | `Delivered uplink block 0: path pouch` | never reached |

Notes:

- `Disconnected ... reason 0x16` is a clean gateway-initiated disconnect after a
  successful sync (not a timeout/crash). `reason 0x08` is a supervision timeout
  and `reason 0x3e` is a failed-to-establish/security failure.
- `Soil sensor ADC sample: raw 0, millivolts 0` means comms are fine but the
  moisture probe is reading nothing - check the probe wiring/insertion separately.

# Latent gateway bug: hard fault on cloud rejection

When Golioth returns a `4.xx` for the device cert, the gateway should log it and
move on. Instead it bus-faults in the `coap_client` thread
(`Faulting instruction address (r15/pc): 0x00000020`, `r14/lr: 0x00000000`) and
resets, producing the reboot loop. Until the device-cert error path in the
pouch gateway cert module is hardened, any cert expiry or cloud hiccup will wedge
the gateway again even though the node side is fine.

# Helper: reading serial logs

The captures in this note were taken with a small PowerShell serial reader at
`C:\ncs_pouch_soil\serial_io.ps1` (open the port, optionally send a shell command,
print everything for N seconds). Node and gateway COM ports re-enumerate when
boards are replugged, so list them first:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
powershell -ExecutionPolicy Bypass -File C:\ncs_pouch_soil\serial_io.ps1 -Port COM12 -Seconds 50
powershell -ExecutionPolicy Bypass -File C:\ncs_pouch_soil\serial_io.ps1 -Port COM17 -Send "kernel reboot" -Seconds 14
```
