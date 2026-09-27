# Calculations

## 1. Voltage Divider (A0 / A1)

Resistors: R1 = 3.9 kΩ, R2 = 1 kΩ

```
V(out) = V(in) × R2 / (R1 + R2)
       = 20 × 1000 / (3900 + 1000)
       = 4.08 V
```

Scale-back factor used in code to recover the real voltage:

```
factor = (R1 + R2) / R2 = 4900 / 1000 = 4.9

realVoltage = analogRead(pin) × (5.0 / 1023.0) × 4.9
```

## 2. Series Capacitor Voltage

Each capacitor charges to the full supply voltage in parallel. In series mode, the pair's total is limited by the supply and load, not doubled:

```
C1 (≈half supply) + C2 (≈half supply) ≈ full supply voltage total
```

## 3. Output Regulation (Q2 emitter follower)

```
V(output) = V(base) − 0.7 V
```

Since the Arduino PWM pin maxes out at 5 V:

```
V(output) max = 5 − 0.7 = 4.3 V
```

## 4. PWM ↔ Voltage Conversion

```
PWM = (targetVoltage / 4.3) × 255
```

Example — target 3.2 V:

```
PWM = (3.2 / 4.3) × 255 ≈ 190
```

## 5. Filter Capacitor — RC Time Constant

```
τ = R × C
```

Example (10 Ω series resistor, 470 µF filter capacitor):

```
τ = 10 × 0.00047 = 4.7 ms
```

PWM period at ~490 Hz:

```
period = 1 / 490 ≈ 2.04 ms
```

Since τ > PWM period, the capacitor cannot fully charge/discharge each cycle and settles at the average (duty-cycle-set) voltage.

## 6. Cutoff Frequency of the Filter

```
f(c) = 1 / (2πRC)
```

Example (10 Ω, 470 µF):

```
f(c) = 1 / (2π × 10 × 0.00047) ≈ 34 Hz
```

Since the PWM switching frequency (~490 Hz) is well above this cutoff, it is attenuated, while the near-0 Hz duty-cycle average passes through largely unaffected.

## 7. Capacitor Drain Rate Under Load

Example that showed why small storage capacitors were insufficient (1000 µF case):

```
Combined series capacitance = (1000 × 1000) / (1000 + 1000) = 500 µF
Current draw ≈ 12 mA

dV/dt = I / C = 0.012 / 0.0005 = 24 V/s
```

This rate of collapse was too fast to behave like a battery, which motivated increasing the storage capacitance significantly.
