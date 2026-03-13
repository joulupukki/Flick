# CloudSeed Reverb Parameters Reference

The Flick pedal uses the **CloudSeed** reverb algorithm for its "Ambient" and "Room" reverb modes. Both modes use the same underlying engine but are initialized with different parameter sets.

In the code:
*   **Ambient (Rubi-Ka Fields):** `initFactoryRubiKaFields()` (Preset 5)
*   **Small Room:** `initFactorySmallRoom()` (Preset 6)

## Parameter Comparison Table

**Key:**  
*   **A** = Ambient Preset Value (0.0 - 1.0)
*   **R** = Room Preset Value (0.0 - 1.0)
*   **Rec** = Recommended for Physical Knob mapping (by Gemini)

### 1. Input & Pre-Delay
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **InputMix** | Mixes input into diffusion network. Higher = dense. | 0.33 | 0.00 | | Affects initial density. |
| **PreDelay** | Delay before reverb starts (0 - 1000ms). | 0.00 | 0.00 | **Yes** | Crucial for keeping attack clear. |
| **HighPass** | Input high-pass filter (20Hz - 1kHz). | 0.00 | 0.00 | | Cuts low-end mud before reverb. |
| **LowPass** | Input low-pass filter (400Hz - 20kHz). | 0.89 | 0.76 | **Yes** | **"Tone"**. Controls brightness entering the tank. |

### 2. Early Reflections (Multitap Delay)
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TapCount** | Number of early reflection taps. | 0.52 | 0.41 | | |
| **TapLength** | Duration of early reflections (0 - 500ms). | 1.00 | 0.44 | | Room uses much shorter early reflections. |
| **TapGain** | Volume of early reflection taps. | 0.90 | 0.88 | | |
| **TapDecay** | Decay rate of early reflections. | 1.00 | 1.00 | | |

### 3. Diffusion (Smearing)
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **DiffusionEnabled** | Enables/disables diffusion stage. | 1.00 | 1.00 | | |
| **DiffusionStages** | Number of allpass filters in series. | 0.86 | 0.71 | | |
| **DiffusionDelay** | Delay time for diffusers (10 - 100ms). | 0.57 | 0.34 | | Room is tighter/shorter. |
| **DiffusionFeedback** | Feedback of diffusers (Resonance). | 0.76 | 0.66 | | High values = metallic/blooming sound. |

### 4. Late Reverb (Main Decay)
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **LineCount** | Number of delay lines (Flick fixed to 1). | - | - | | |
| **LineDelay** | Length of delay lines (20 - 1000ms). | 0.69 | 0.51 | | Determines "room size". |
| **LineDecay** | Feedback of delay lines. | 0.83 | 0.30 | **Yes** | **"Decay" / "Size"**. The most important control. |
| **LateDiffusionEnabled** | Enables diffusion in late reverb loop. | 1.00 | 1.00 | | |
| **LateDiffusionStages** | Number of allpass filters in late loop. | 0.71 | 0.43 | | |
| **LateDiffusionDelay** | Delay time of late diffusers. | 0.69 | 0.23 | | |
| **LateDiffusionFeedback** | Feedback of late diffusers. | 0.71 | 0.59 | **Yes** | **"Texture"**. Controls smooth vs grainy tail. |

### 5. Output EQ
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **PostLowShelfGain** | Low shelf boost/cut at output. | 0.88 | 0.88 | | |
| **PostLowShelfFreq** | Frequency for low shelf (20Hz - 1kHz). | 0.19 | 0.19 | | |
| **PostHighShelfGain** | High shelf boost/cut at output. | 0.72 | 0.88 | | Room is brighter at the output stage. |
| **PostHighShelfFreq** | Frequency for high shelf (400Hz - 20kHz). | 0.52 | 0.59 | | |
| **PostCutoffFreq** | Hard output low-pass (400Hz - 20kHz). | 0.80 | 0.80 | | |

### 6. Modulation (The "Cloud" Effect)
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **EarlyDiffModAmount** | Mod depth for early reflections. | 0.13 | 0.13 | | |
| **EarlyDiffModRate** | Mod speed for early reflections. | 0.26 | 0.29 | | |
| **LineModAmount** | LFO depth modulation on delay lines. | 0.05 | 0.19 | **Yes** | **"Mod Depth"**. Adds chorus/pitch movement. |
| **LineModRate** | LFO speed for main delay lines. | 0.21 | 0.23 | **Yes** | **"Mod Speed"**. |
| **LateDiffModAmount** | Mod depth for late diffusers. | 0.33 | 0.12 | | Ambient has much more modulation here. |
| **LateDiffModRate** | Mod speed for late diffusers. | 0.36 | 0.29 | | |

### 7. Output Mix
| Parameter | Description | A | R | Rec | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **DryOut** | Level of dry signal. | 0.85 | 1.00 | | *Overridden to 0 by Flick for wet-only processing*. |
| **PredelayOut** | Level of signal after pre-delay. | 0.00 | 0.00 | | |
| **EarlyOut** | Level of early reflections. | 0.00 | 0.86 | | **Key Difference!** Ambient is all "tail", Room has strong reflections. |
| **MainOut** | Level of late reverb (tail). | 0.91 | 0.91 | | |
