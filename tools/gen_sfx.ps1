$ErrorActionPreference = 'Stop'

$csharp = @"
using System;
using System.IO;

public class DuffSfxGen {
    const int SR = 11025;
    static uint rng = 0x1994CAFEu;

    static double White() {
        rng = rng * 1664525u + 1013904223u;
        return ((int)((rng >> 9) & 0x7FFFFFu) / 4194303.5) - 1.0;
    }

    static void SaveWav16(string path, double[] samples) {
        int n = samples.Length;
        using (FileStream fs = new FileStream(path, FileMode.Create, FileAccess.Write))
        using (BinaryWriter bw = new BinaryWriter(fs)) {
            int dataBytes = n * 2;
            bw.Write(new char[] { 'R', 'I', 'F', 'F' });
            bw.Write(36 + dataBytes);
            bw.Write(new char[] { 'W', 'A', 'V', 'E' });
            bw.Write(new char[] { 'f', 'm', 't', ' ' });
            bw.Write(16);
            bw.Write((ushort)1);       // PCM
            bw.Write((ushort)1);       // Mono
            bw.Write(SR);              // 11,025 Hz (exact 1:1 Sound Blaster rate)
            bw.Write(SR * 2);          // ByteRate
            bw.Write((ushort)2);       // BlockAlign
            bw.Write((ushort)16);      // 16-bit signed PCM
            bw.Write(new char[] { 'd', 'a', 't', 'a' });
            bw.Write(dataBytes);

            for (int i = 0; i < n; i++) {
                // Soft-clip saturation + TPDF dither for clean 8-bit SB quantization
                double x = samples[i];
                if (x > 1.2) x = 1.2;
                if (x < -1.2) x = -1.2;
                x = Math.Tanh(x * 1.15);
                double dither = (White() - White()) * (1.0 / 256.0);
                int s = (int)Math.Round((x + dither) * 32000.0);
                if (s > 32767) s = 32767;
                if (s < -32768) s = -32768;
                bw.Write((short)s);
            }
        }
    }

    public static void GenerateAll(string dir) {
        Directory.CreateDirectory(dir);

        // 1. CLICK.WAV - Tactile 90s mechanical switch click
        {
            int n = (int)(SR * 0.026);
            double[] b = new double[n];
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                b[i] = Math.Sin(2 * Math.PI * 1950 * t) * Math.Exp(-t * 220) * 0.75
                     + Math.Sin(2 * Math.PI * 620 * t)  * Math.Exp(-t * 140) * 0.55;
            }
            SaveWav16(Path.Combine(dir, "CLICK.WAV"), b);
        }

        // 2 & 3. STEPA.WAV & STEPB.WAV - Alternating Left/Right tactical boot footsteps
        for (int foot = 0; foot < 2; foot++) {
            int n = (int)(SR * 0.095);
            double[] b = new double[n];
            double ph = 0, lp = 0, bp = 0;
            double f0 = (foot == 0) ? 142.0 : 162.0;
            double cut = (foot == 0) ? 0.34 : 0.42;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                // Heel strike (0..25ms) + sole roll/scuff (18..75ms)
                double heelEnv = Math.Exp(-t * 58.0);
                double soleEnv = (t > 0.014) ? Math.Exp(-(t - 0.014) * 44.0) * 0.65 : 0.0;
                double f = f0 * Math.Exp(-t * 24.0) + 42.0;
                ph += 2 * Math.PI * f / SR;

                double w = White();
                lp += cut * (w - lp);
                bp += 0.22 * ((w - lp) - bp);

                b[i] = Math.Sin(ph) * 0.82 * heelEnv
                     + lp * 0.55 * heelEnv
                     + bp * 0.75 * soleEnv;
            }
            SaveWav16(Path.Combine(dir, foot == 0 ? "STEPA.WAV" : "STEPB.WAV"), b);
        }

        // 4. DOOR.WAV - Pneumatic door slide + heavy latch clunk
        {
            int n = (int)(SR * 0.28);
            double[] b = new double[n];
            double lp = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double hissEnv = (t < 0.20) ? Math.Sin(Math.PI * (t / 0.20)) : 0.0;
                double cut = 0.45 - 0.25 * (t / 0.28);
                lp += cut * (White() - lp);
                double clunk = 0.0;
                if (t >= 0.19) {
                    double tc = t - 0.19;
                    clunk = (Math.Sin(2 * Math.PI * 175 * tc) * 0.75 + Math.Sin(2 * Math.PI * 410 * tc) * 0.45) * Math.Exp(-tc * 45.0);
                }
                b[i] = lp * 0.65 * hissEnv + clunk;
            }
            SaveWav16(Path.Combine(dir, "DOOR.WAV"), b);
        }

        // 5. RIFLE.WAV - Punchy 1994 Assault Rifle gunshot + street slapback + brass ping
        {
            int n = (int)(SR * 0.34);
            double[] b = new double[n];
            double ph = 0, lp = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double crackEnv = Math.Exp(-t * 48.0);
                double bodyEnv  = Math.Exp(-t * 14.0);
                double echoEnv  = (t > 0.045) ? Math.Exp(-(t - 0.045) * 16.0) * 0.32 : 0.0;

                double f = 230.0 * Math.Exp(-t * 28.0) + 46.0;
                ph += 2 * Math.PI * f / SR;

                double w = White();
                lp += 0.48 * (w - lp);

                double casing = (t > 0.12) ? Math.Sin(2 * Math.PI * 3650 * (t - 0.12)) * Math.Exp(-(t - 0.12) * 55.0) * 0.18 : 0.0;
                b[i] = Math.Sin(ph) * 0.95 * crackEnv
                     + w * 0.90 * Math.Exp(-t * 85.0)
                     + lp * (0.75 * bodyEnv + echoEnv)
                     + casing;
            }
            SaveWav16(Path.Combine(dir, "RIFLE.WAV"), b);
        }

        // 6. LASER.WAV - Coopersburg Prototype Laser Rifle (high-energy sci-fi beam)
        {
            int n = (int)(SR * 0.28);
            double[] b = new double[n];
            double ph1 = 0, ph2 = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double f = 2550.0 * Math.Exp(-t * 10.5) + 195.0 + 35.0 * Math.Sin(2 * Math.PI * 55.0 * t);
                double env = Math.Exp(-t * 8.0);
                ph1 += 2 * Math.PI * f / SR;
                ph2 += 2 * Math.PI * (f * 1.505) / SR;
                b[i] = (Math.Sin(ph1) * 0.72 + Math.Sin(ph2) * 0.35 + White() * 0.15 * Math.Exp(-t * 35.0)) * env;
            }
            SaveWav16(Path.Combine(dir, "LASER.WAV"), b);
        }

        // 7. PLASMA.WAV - Classic X-COM Heavy Plasma discharge (FM sci-fi blast + ionized sizzle)
        {
            int n = (int)(SR * 0.38);
            double[] b = new double[n];
            double phC = 0, phM = 0, lp = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double fc = 760.0 * Math.Exp(-t * 5.8) + 88.0;
                double fm = 118.0 + 40.0 * Math.Exp(-t * 12.0);
                double env = Math.Exp(-t * 5.6);
                phC += 2 * Math.PI * fc / SR;
                phM += 2 * Math.PI * fm / SR;
                lp += 0.35 * (White() - lp);
                b[i] = (Math.Sin(phC + 2.2 * Math.Sin(phM)) * 0.80 + lp * 0.42) * env;
            }
            SaveWav16(Path.Combine(dir, "PLASMA.WAV"), b);
        }

        // 8. EXPLODE.WAV - Deep HE concussion blast + debris roar
        {
            int n = (int)(SR * 0.85);
            double[] b = new double[n];
            double ph = 0, lp1 = 0, lp2 = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double env = Math.Exp(-t * 3.1);
                double f = 115.0 * Math.Exp(-t * 4.5) + 28.0;
                ph += 2 * Math.PI * f / SR;
                double cut = 0.42 * Math.Exp(-t * 2.2) + 0.08;
                double w = White();
                lp1 += cut * (w - lp1);
                lp2 += cut * (lp1 - lp2);
                double crackle = ((i % 97) < 4) ? w * 0.35 * Math.Exp(-t * 2.5) : 0.0;
                b[i] = (lp2 * 1.45 + Math.Sin(ph) * 0.78 * Math.Exp(-t * 5.0)) * env + crackle;
            }
            SaveWav16(Path.Combine(dir, "EXPLODE.WAV"), b);
        }

        // 9. GLASS.WAV - Shattering Diner / Brick window panes + falling shards
        {
            int n = (int)(SR * 0.32);
            double[] b = new double[n];
            double[] shardTimes = new double[] { 0.0, 0.035, 0.075, 0.115, 0.165, 0.215 };
            double[] shardFreqs = new double[] { 2350, 3180, 2790, 3890, 3420, 4210 };
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double s = White() * 0.75 * Math.Exp(-t * 28.0);
                for (int k = 0; k < shardTimes.Length; k++) {
                    if (t >= shardTimes[k]) {
                        double dt = t - shardTimes[k];
                        s += (Math.Sin(2 * Math.PI * shardFreqs[k] * dt) + 0.5 * Math.Sin(2 * Math.PI * (shardFreqs[k] * 1.37) * dt))
                           * Math.Exp(-dt * 26.0) * 0.34;
                    }
                }
                b[i] = s;
            }
            SaveWav16(Path.Combine(dir, "GLASS.WAV"), b);
        }

        // 10. HIT.WAV - Kinetic bullet / plasma impact thud
        {
            int n = (int)(SR * 0.16);
            double[] b = new double[n];
            double ph = 0, lp = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double env = Math.Exp(-t * 22.0);
                ph += 2 * Math.PI * (245.0 * Math.Exp(-t * 18.0) + 52.0) / SR;
                lp += 0.38 * (White() - lp);
                b[i] = (Math.Sin(ph) * 0.85 + lp * 0.65) * env;
            }
            SaveWav16(Path.Combine(dir, "HIT.WAV"), b);
        }

        // 11. ALIENDIE.WAV - Iconic 1994 X-COM Sectoid high-pitched warbling shriek!
        {
            int n = (int)(SR * 0.64);
            double[] b = new double[n];
            double ph1 = 0, ph2 = 0, lp = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double attack = (t < 0.025) ? (t / 0.025) : 1.0;
                double env = attack * Math.Sin(Math.PI * Math.Pow((double)i / n, 0.78)) * 0.95;
                // Piercing alien vocal screech plunging downward with rapid laryngeal trill
                double trill = 78.0 * Math.Sin(2 * Math.PI * 39.0 * t) + 32.0 * Math.Sin(2 * Math.PI * 17.0 * t);
                double f1 = 1420.0 * Math.Exp(-t * 2.35) + 215.0 + trill;
                double f2 = f1 * 1.414 + 45.0 * Math.Cos(2 * Math.PI * 29.0 * t);
                ph1 += 2 * Math.PI * f1 / SR;
                ph2 += 2 * Math.PI * f2 / SR;
                lp += 0.36 * (White() - lp);

                double screech = Math.Sin(ph1 + 1.15 * Math.Sin(ph1 * 2.0)) * 0.62
                               + Math.Sin(ph2) * 0.34
                               + lp * (0.28 + 0.20 * Math.Sin(2 * Math.PI * 39.0 * t));
                b[i] = screech * env;
            }
            SaveWav16(Path.Combine(dir, "ALIENDIE.WAV"), b);
        }

        // 12. FLOATDIE.WAV - Cybernetic Floater death shriek + sputtering anti-grav thruster!
        {
            int n = (int)(SR * 0.68);
            double[] b = new double[n];
            double phV = 0, phT = 0, bp = 0, lp = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double attack = (t < 0.02) ? (t / 0.02) : 1.0;
                double env = attack * Math.Sin(Math.PI * Math.Pow((double)i / n, 0.82));
                // Vocoder-like metallic shriek + spinning-down hover turbine
                double fv = 980.0 * Math.Exp(-t * 2.1) + 155.0 + 42.0 * Math.Sin(2 * Math.PI * 52.0 * t);
                double ft = 1780.0 * Math.Exp(-t * 3.4) + 120.0;
                phV += 2 * Math.PI * fv / SR;
                phT += 2 * Math.PI * ft / SR;
                double w = White();
                lp += 0.28 * (w - lp);
                bp += 0.18 * ((w - lp) - bp);

                double cyb = Math.Sin(phV + 1.65 * Math.Sin(phV * 1.5)) * 0.58
                           + Math.Sin(phT) * 0.26 * Math.Exp(-t * 1.8)
                           + bp * 0.48;
                b[i] = cyb * env;
            }
            SaveWav16(Path.Combine(dir, "FLOATDIE.WAV"), b);
        }

        // 13. MUTONDIE.WAV - Hulking Muton guttural death roar + heavy armor thud!
        {
            int n = (int)(SR * 0.76);
            double[] b = new double[n];
            double ph = 0, y1a = 0, y2a = 0, y1b = 0, y2b = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double attack = (t < 0.03) ? (t / 0.03) : 1.0;
                double env = attack * Math.Sin(Math.PI * Math.Pow((double)i / n, 0.75));
                double f0 = (t < 0.10) ? (135.0 + t * 220.0) : (157.0 * Math.Exp(-(t - 0.10) * 1.35) + 56.0);
                f0 += 9.0 * Math.Sin(2 * Math.PI * 34.0 * t);
                ph += f0 / SR;
                if (ph >= 1.0) ph -= 1.0;

                // Rich glottal sawtooth + sub-growl modulation
                double glottal = (2.0 * ph - 1.0) * (0.72 + 0.28 * Math.Sin(2 * Math.PI * 34.0 * t))
                               + White() * 0.32;

                // Monster vocal-tract formants (F1=480->340 Hz, F2=960->720 Hz)
                double r1 = Math.Exp(-Math.PI * 130.0 / SR);
                double c1 = 2.0 * r1 * Math.Cos(2 * Math.PI * (480.0 - 160.0 * (t / 0.76)) / SR);
                double out1 = (1.0 - r1) * glottal + c1 * y1a - (r1 * r1) * y2a;
                y2a = y1a; y1a = out1;

                double r2 = Math.Exp(-Math.PI * 190.0 / SR);
                double c2 = 2.0 * r2 * Math.Cos(2 * Math.PI * (960.0 - 220.0 * (t / 0.76)) / SR);
                double out2 = (1.0 - r2) * glottal + c2 * y1b - (r2 * r2) * y2b;
                y2b = y1b; y1b = out2;

                // Heavy bio-armor body slam at t = 0.56s!
                double slam = (t >= 0.55) ? Math.Sin(2 * Math.PI * 68.0 * (t - 0.55)) * Math.Exp(-(t - 0.55) * 26.0) * 0.55 : 0.0;
                b[i] = (out1 * 2.1 + out2 * 1.5 + Math.Sin(2 * Math.PI * ph) * 0.38) * env + slam;
            }
            SaveWav16(Path.Combine(dir, "MUTONDIE.WAV"), b);
        }

        // 14. MANDIE.WAV - 1994 X-COM style Male Operative agonized scream ("AAARRRGH!!")
        {
            int n = (int)(SR * 0.68);
            double[] b = new double[n];
            double ph = 0;
            double f1_1 = 0, f1_2 = 0, f2_1 = 0, f2_2 = 0, f3_1 = 0, f3_2 = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double attack = (t < 0.025) ? (t / 0.025) : 1.0;
                double env = attack * ((t < 0.46) ? 1.0 : Math.Cos(0.5 * Math.PI * ((t - 0.46) / 0.22)));
                if (env < 0) env = 0;

                // Strained chest-voice F0: surges 215->262 Hz in agony, drops to ragged 112 Hz groan
                double f0 = (t < 0.09) ? (215.0 + (t / 0.09) * 47.0)
                                       : (152.0 * Math.Exp(-(t - 0.09) * 2.2) + 110.0);
                f0 += 6.5 * Math.Sin(2 * Math.PI * 28.0 * t) + 3.0 * Math.Sin(2 * Math.PI * 63.0 * t);
                ph += f0 / SR;
                if (ph >= 1.0) ph -= 1.0;

                // Sharp glottal vocal-cord impulse + throat rasp aspiration
                double pulse = Math.Exp(-ph * 5.5) * Math.Sin(2 * Math.PI * ph) * 2.2
                             + (1.0 - 2.0 * ph) * 0.65
                             + White() * (0.34 + 0.22 * Math.Sin(2 * Math.PI * ph));

                // Open-throat male "AAAH -> UGH" vocal formants (F1: 760->520, F2: 1260->980, F3: 2420)
                double prog = t / 0.68;
                double r1 = Math.Exp(-Math.PI * 110.0 / SR);
                double o1 = (1.0 - r1) * pulse + (2.0 * r1 * Math.Cos(2 * Math.PI * (760.0 - 240.0 * prog) / SR)) * f1_1 - (r1 * r1) * f1_2;
                f1_2 = f1_1; f1_1 = o1;

                double r2 = Math.Exp(-Math.PI * 150.0 / SR);
                double o2 = (1.0 - r2) * pulse + (2.0 * r2 * Math.Cos(2 * Math.PI * (1260.0 - 280.0 * prog) / SR)) * f2_1 - (r2 * r2) * f2_2;
                f2_2 = f2_1; f2_1 = o2;

                double r3 = Math.Exp(-Math.PI * 220.0 / SR);
                double o3 = (1.0 - r3) * pulse + (2.0 * r3 * Math.Cos(2 * Math.PI * 2420.0 / SR)) * f3_1 - (r3 * r3) * f3_2;
                f3_2 = f3_1; f3_1 = o3;

                double thud = (t >= 0.52) ? Math.Sin(2 * Math.PI * 82.0 * (t - 0.52)) * Math.Exp(-(t - 0.52) * 32.0) * 0.35 : 0.0;
                b[i] = (o1 * 1.85 + o2 * 1.45 + o3 * 0.65) * env + thud;
            }
            SaveWav16(Path.Combine(dir, "MANDIE.WAV"), b);
        }

        // 15. FEMDIE.WAV - 1994 X-COM style Female Operative agonized scream ("AAAAHHH!!")
        {
            int n = (int)(SR * 0.66);
            double[] b = new double[n];
            double ph = 0;
            double f1_1 = 0, f1_2 = 0, f2_1 = 0, f2_2 = 0, f3_1 = 0, f3_2 = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double attack = (t < 0.022) ? (t / 0.022) : 1.0;
                double env = attack * ((t < 0.44) ? 1.0 : Math.Cos(0.5 * Math.PI * ((t - 0.44) / 0.22)));
                if (env < 0) env = 0;

                // High dramatic female cry F0: surges 415->495 Hz, falls to 225 Hz
                double f0 = (t < 0.085) ? (415.0 + (t / 0.085) * 80.0)
                                        : (270.0 * Math.Exp(-(t - 0.085) * 2.1) + 225.0);
                f0 += 11.0 * Math.Sin(2 * Math.PI * 31.0 * t) + 4.5 * Math.Sin(2 * Math.PI * 71.0 * t);
                ph += f0 / SR;
                if (ph >= 1.0) ph -= 1.0;

                double pulse = Math.Exp(-ph * 5.0) * Math.Sin(2 * Math.PI * ph) * 2.1
                             + (1.0 - 2.0 * ph) * 0.55
                             + White() * (0.28 + 0.18 * Math.Sin(2 * Math.PI * ph));

                // Open female "AAAH" vocal formants (F1: 890->650, F2: 1620->1280, F3: 2880)
                double prog = t / 0.66;
                double r1 = Math.Exp(-Math.PI * 125.0 / SR);
                double o1 = (1.0 - r1) * pulse + (2.0 * r1 * Math.Cos(2 * Math.PI * (890.0 - 240.0 * prog) / SR)) * f1_1 - (r1 * r1) * f1_2;
                f1_2 = f1_1; f1_1 = o1;

                double r2 = Math.Exp(-Math.PI * 170.0 / SR);
                double o2 = (1.0 - r2) * pulse + (2.0 * r2 * Math.Cos(2 * Math.PI * (1620.0 - 340.0 * prog) / SR)) * f2_1 - (r2 * r2) * f2_2;
                f2_2 = f2_1; f2_1 = o2;

                double r3 = Math.Exp(-Math.PI * 240.0 / SR);
                double o3 = (1.0 - r3) * pulse + (2.0 * r3 * Math.Cos(2 * Math.PI * 2880.0 / SR)) * f3_1 - (r3 * r3) * f3_2;
                f3_2 = f3_1; f3_1 = o3;

                double thud = (t >= 0.50) ? Math.Sin(2 * Math.PI * 92.0 * (t - 0.50)) * Math.Exp(-(t - 0.50) * 34.0) * 0.30 : 0.0;
                b[i] = (o1 * 1.85 + o2 * 1.40 + o3 * 0.62) * env + thud;
            }
            SaveWav16(Path.Combine(dir, "FEMDIE.WAV"), b);
        }

        // 16. ALERT.WAV - Two-tone X-COM motion scanner / alien spotted alert
        {
            int n = (int)(SR * 0.22);
            double[] b = new double[n];
            double ph = 0;
            for (int i = 0; i < n; i++) {
                double t = (double)i / SR;
                double f = (i < n / 2) ? 784.0 : 1174.66;
                double localT = (i < n / 2) ? t : (t - 0.11);
                double env = Math.Exp(-localT * 10.0);
                ph += 2 * Math.PI * f / SR;
                b[i] = (Math.Sin(ph) * 0.72 + Math.Sin(ph * 2) * 0.22) * env;
            }
            SaveWav16(Path.Combine(dir, "ALERT.WAV"), b);
        }
    }
}
"@

Add-Type -TypeDefinition $csharp
[DuffSfxGen]::GenerateAll((Join-Path $PSScriptRoot "..\assets"))
Write-Host "Generated 16 tactical WAV sound effects in assets/!"
