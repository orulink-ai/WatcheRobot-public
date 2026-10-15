English | [简体中文](../../guide/reproduction.md)

# DIY Build Guide

Start with parts and build a WatcheRobot that powers on, connects to a network, displays expressions, and performs movements. Follow this order: **confirm parts → print structures and fabricate PCBs → source remaining parts → assemble and wire → install firmware and SD resources → verify the robot**. Printing and PCB fabrication can run in parallel; software setup is not the first step.

This build uses a purchased **Watcher head module, custom servos, and the published body hardware design**. It does not recreate the head from bare chips or require compiling official embedded source code. Already have a working robot? See [Getting started](getting-started.md).

## 1. Confirm the parts you need {#parts}

Download the [assembly BOM](../../../../hardware/pcb/bom/BOM_Assembly_整机组装料.xlsx), then distinguish purchased modules from fabricated parts below. The workbook is large; download it for viewing. Use the BOM and matching assembly illustrations for models, quantities, screw specifications, and cables.

| Part | How to obtain it | Confirm before fabrication |
| --- | --- | --- |
| Watcher head | Purchase the corresponding module, or ask the project maintainers about sourcing | Compatibility with the body structure, interfaces, and paired firmware; a generic ESP32 board is not a replacement |
| Custom servos | Ask maintainers for the model and sourcing options | Matching dimensions, connections, and feedback interface; do not select generic servos by appearance alone |
| Body and base structures | Download models and print them yourself or use a printing service | Model revision, quantities, and printing requirements |
| PCBs | Fabricate from the published design, then solder or order SMT assembly | A bare PCB is not a populated board |
| Cables, screws, and other assembly parts | Source from the assembly BOM; ask maintainers about a cable kit | Matching connectors, pin order, lengths, screw sizes, and mounting locations |
| Battery and power parts | Source according to the BOM and assembly instructions | Correct specifications, connector, and polarity; fitting connectors do not establish compatibility |

If custom-part specifications or sourcing are unclear, contact maintainers through the [community](../products/watcherobot/community.md) before ordering the rest. Prepare screwdrivers, tweezers, and a multimeter; add soldering equipment if assembling boards yourself. ST-LINK V2, a USB data cable, an SD card, and a card reader are needed later.

## 2. Download models and print the structures {#printing}

1. Open the [MakerWorld model page](https://makerworld.com.cn/zh/models/2911544-watcherobot) and obtain the printable models and accompanying instructions.
2. Follow the publisher's part quantities, material, orientation, and support requirements when printing or commissioning a service. Do not slice the complete assembly STEP as a single printable part.
3. Remove supports and burrs, check the parts list, and test-fit screw holes, PCB mounts, and servo mounting locations. Resolve visible warping or interference before final assembly.

Use the repository's [complete assembly STEP](../../../../hardware/3d-models/exports/WatcherRobot-mian.stp) to inspect component relationships or edit the structure. It is a structural reference; confirm printing settings in the corresponding model instructions.

**Completion check:** you have counted and test-fitted the physical structures, not merely downloaded models.

## 3. Fabricate and populate the PCBs {#pcb}

Open the [OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl) and locate the required boards. The repository contains these four board families; use the assembly BOM for actual quantities:

| Board | Repository filename identifier |
| --- | --- |
| STM32 feedback-servo controller (body board) | STM32Servo |
| Foot charging board | FootCharger |
| Wireless charging base | WirelessBase |
| Side light board | SideLight |

### Fabrication

1. In the project's design list, select the target board and choose “在编辑器中打开” to open it in the JLCEDA editor.
2. Verify the revision and use “导出 → PCB/FPC 一键打样” to generate production files and enter ordering. If you modified the circuit or PCB, check design rules and connectivity first.
3. Confirm layers, thickness, dimensions, quantity, and manufacturing requirements against that board's design. Do not reuse one board's parameters for every board.
4. Review the preview and quotation before confirming an order yourself.

See [How to obtain the PCBs](https://mcnsslrwxv50.feishu.cn/wiki/YSAlwY4EZiuR0jkYcu2ctHzKn3R) for illustrated steps using the STM32 board (Chinese). Platform controls may change.

Alternatively, provide the matching [Gerber files](../../../../hardware/pcb/gerber/) to a PCB manufacturer. Gerbers describe bare-board fabrication. Editable projects are in [pcb-source](../../../../hardware/pcb/pcb-source/); schematic and layout PDFs are inspection references, not substitutes for production Gerbers.

### Soldering or SMT assembly

- **Outsourced SMT:** use the same board revision's [BOM](../../../../hardware/pcb/bom/) and [CPL coordinates](../../../../hardware/pcb/cpl/). Check models, packages, mounting sides, orientation, and polarity; confirm electrical and mechanical compatibility before accepting substitutions.
- **Hand soldering:** populate according to the BOM, [schematics](../../../../hardware/pcb/schematic/), and [layouts](../../../../hardware/pcb/layout/). Partial SMT orders still require the remaining components to be soldered after delivery.

Inspect for incorrect, missing, reversed, poorly soldered, or bridged components. With power disconnected, check for abnormal shorts between power and ground. Do not connect the battery or head until these checks are complete.

**Completion check:** you have populated, inspected boards, not just bare PCBs.

## 4. Inventory, assemble, and wire {#assembly}

While fabrication is underway, source the remaining parts from step 1. Sort deliveries and verify models, quantities, and connectors before assembly. Do not improvise with unidentified screws or cables.

Open the [illustrated robot assembly instructions](https://mcnsslrwxv50.feishu.cn/wiki/PNhqwdCVEiqizvkelq9c2q7Qnth) (Chinese) and use the actual photographs for the tasks below. Hole positions, ribbon orientation, pin order, and screw lengths must follow the corresponding illustrations; this summary and wire colors alone are not wiring instructions.

| Assembly area | Work to complete | Check |
| --- | --- | --- |
| Head and body board | Route cables, connect the head and board, and secure the corresponding structures | Match photographs and board labels; do not pinch cables |
| Servos and body mechanism | Mount servos, connect the body board, and check reset and mounting positions as illustrated | Do not force servo rotation or tighten a horn to correct misalignment |
| Lights and touch parts | Mount boards and connect ribbons and touch components | Correct ribbon orientation and latches; conductive parts must not contact unintended pads |
| Feet and base | Fit structures, bottom PCB, and associated wiring | Align holes and leave cable movement allowance outside pinch points |
| Battery and enclosure | Verify power wiring, arrange internal cables, and secure covers | No battery damage or compression; screws must not press into the battery or boards |

Disconnect power during assembly and ribbon changes. If a servo reset check requires power, first inspect wiring and confirm the body board has working paired firmware. For a new unflashed board, complete STM32 flashing in the next section before the reset check. A new board not responding does not establish servo failure.

Keep flashing ports and the SD card accessible before closing the enclosure. Stop and ask maintainers if pin order or mounting orientation cannot be established; do not experiment with powered connections.

**Completion check:** secure structures, unpinched cables, verified polarity and connectors, and no obvious mechanical interference.

## 5. Install firmware and SD resources {#firmware}

Prepare the computer environment now; SDK installation is not required before printing or purchasing.

1. Download and extract a code ZIP from the [public repository](https://github.com/orulink-ai/WatcheRobot-public) for flashing tools, or clone it if familiar with Git. Reproducing the official firmware does not require initializing the Python SDK submodule.
2. Obtain matching STM32 firmware, paired head firmware, and SD resources from [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases), following the release's compatibility notes.
3. Follow the [full flashing guide](../../../../docs/flashing.md#full) to prepare the environment, flash STM32 with ST-LINK V2, and flash the ESP32-S3 / Himax head over USB.
4. Follow the [SD resource guide](../../../../docs/sd-card-assets.md) to format and write the card. Return it to the head with power disconnected.

An AI coding assistant can read the [release flashing Skill](../../../../skills/watche-release-flash/SKILL.md) to assist. Browser flashing only updates ESP32-S3; it does not replace the full installation here.

**Completion check:** writes and verification succeed for the targets and SD resources are installed. This does not yet prove all hardware works.

## 6. Power on and verify the robot {#verify}

Place the robot on a safe, level surface with clearance for movement. Check the display, abnormal heat, unusual noise, and mechanical binding at startup. Disconnect power and investigate abnormalities instead of repeatedly sending movements.

Follow [Getting started](getting-started.md) for networking and connection, then perform the [first-run check](../../../../docs/action-test.md):

- The display shows the main interface and expressions.
- A phone or computer actually connects to the device.
- Lights respond and small movements have the expected direction, without collisions or sustained stalling.
- If using audio/video, verify actual speaker, microphone, and camera results, not just a connected status.

This completes the path from parts to a usable robot. Application development is optional; continue with the [SDK quick start](../../../../docs/sdk.md).

> Detailed PCB ordering and assembly photographs currently remain in the linked Feishu guides and may require access permission. This page covers the full fabrication workflow and resource entry points, not every illustrated mounting location or pin connection. If those guides cannot be opened, obtain the materials from maintainers before assembly.
