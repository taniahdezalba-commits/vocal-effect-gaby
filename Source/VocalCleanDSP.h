// =============================================================================
//  Vocal Effect Gaby - motor de limpieza de voz (sin dependencias de JUCE)
//  Gaby Records
//
//  Reducción de ruido espectral en tiempo real para voz:
//   - STFT con 75 % de traslape (ventanas raíz de Hann)
//   - Estimación automática del ruido de fondo por bin (seguidor de mínimos)
//   - Filtro de Wiener con SNR a priori "decision-directed" (Ephraim-Malah)
//   - Suavizado en tiempo y frecuencia para evitar "ruido musical"
//   - 4 bandas con cantidad de limpieza independiente
//   - Compuerta entre frases (cuando no hay voz)
//   - Modo "Ambiente": se queda con el ruido y quita la voz
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace gabyvx
{

static constexpr double kPi = 3.14159265358979323846;

// ----------------------------------------------------------------------------
//  FFT radix-2 (in-place, compleja)
// ----------------------------------------------------------------------------
class FFT
{
public:
    void prepare (int n)
    {
        size = n;
        bitrev.assign ((size_t) n, 0);
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            bitrev[(size_t) i] = r;
        }
        twiddle.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
            twiddle[(size_t) i] = std::polar (1.0f, (float) (-2.0 * kPi * i / n));
    }

    void forward (std::complex<float>* x) const { run (x, false); }
    void inverse (std::complex<float>* x) const
    {
        run (x, true);
        const float s = 1.0f / (float) size;
        for (int i = 0; i < size; ++i) x[i] *= s;
    }

private:
    void run (std::complex<float>* x, bool inv) const
    {
        for (int i = 0; i < size; ++i)
            if (i < bitrev[(size_t) i]) std::swap (x[i], x[bitrev[(size_t) i]]);

        for (int len = 2; len <= size; len <<= 1)
        {
            const int half = len / 2, step = size / len;
            for (int i = 0; i < size; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = twiddle[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const auto u = x[i + j];
                    const auto v = x[i + j + half] * w;
                    x[i + j] = u + v;
                    x[i + j + half] = u - v;
                }
        }
    }

    int size = 0;
    std::vector<int> bitrev;
    std::vector<std::complex<float>> twiddle;
};

// ----------------------------------------------------------------------------
//  Parámetros
// ----------------------------------------------------------------------------
struct Settings
{
    double clarity = 0.6;                         // 0..1  cantidad de limpieza principal
    double ambience = 0.0;                        // 0..1  cuánto ambiente/reflexiones conservar
    double gateDepthDb = 0.0;                     // 0..40 dB de atenuación entre frases
    std::array<double, 4> bands { 1, 1, 1, 1 };   // 0..2  cantidad por banda
    bool ambienceMode = false;                    // true = quita la voz, deja el ambiente
};

static constexpr int kDisplayBands = 48;

// ----------------------------------------------------------------------------
//  Motor por canal
// ----------------------------------------------------------------------------
class ChannelProcessor
{
public:
    void prepare (int fftSize, double sampleRate)
    {
        n = fftSize;
        hop = n / 4;
        bins = n / 2 + 1;
        sr = sampleRate;

        inFifo.assign ((size_t) n, 0.0f);
        outAcc.assign ((size_t) n * 2, 0.0f);
        frame.assign ((size_t) n, {});
        noise.assign ((size_t) bins, 0.0f);
        smoothP.assign ((size_t) bins, 0.0f);
        prevClean.assign ((size_t) bins, 0.0f);
        gainSm.assign ((size_t) bins, 1.0f);
        gainTmp.assign ((size_t) bins, 1.0f);
        power.assign ((size_t) bins, 0.0f);

        window.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            window[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * kPi * i / n));  // raíz de Hann periódica

        fft.prepare (n);
        writePos = 0;
        hopCount = 0;
        outReadPos = 0;
        framesSeen = 0;
        gateGain = 1.0f;

        // subida lenta del piso de ruido: ~4 dB por segundo
        const double hopSec = (double) hop / sr;
        noiseRise = (float) std::pow (10.0, 4.0 * hopSec / 10.0);
        gateAttack = (float) (1.0 - std::exp (-hopSec / 0.005));
        gateRelease = (float) (1.0 - std::exp (-hopSec / 0.15));

        // cantidad de banda por bin, con transiciones suaves (en octavas)
        bandWeights.assign ((size_t) bins, { 0, 0, 0, 0 });
        const double edges[3] = { 250.0, 2000.0, 6000.0 };
        for (int k = 0; k < bins; ++k)
        {
            const double f = std::max (20.0, (double) k * sr / n);
            // pertenencia suave (logística en octavas):
            // graves = s0, medios = (1-s0)*s1, presencia = (1-s1)*s2, aire = 1-s2
            std::array<double, 4> w {};
            const double s0 = 1.0 / (1.0 + std::exp ((std::log2 (f) - std::log2 (edges[0])) * 6.0));
            const double s1 = 1.0 / (1.0 + std::exp ((std::log2 (f) - std::log2 (edges[1])) * 6.0));
            const double s2 = 1.0 / (1.0 + std::exp ((std::log2 (f) - std::log2 (edges[2])) * 6.0));
            w = { s0, (1.0 - s0) * s1, (1.0 - s1) * s2, 1.0 - s2 };
            const double sum = w[0] + w[1] + w[2] + w[3];
            for (auto& v : w) v /= sum;
            bandWeights[(size_t) k] = { (float) w[0], (float) w[1], (float) w[2], (float) w[3] };
        }

        voiceLo = std::max (1, (int) (200.0 * n / sr));
        voiceHi = std::min (bins - 1, (int) (4000.0 * n / sr));
    }

    int latency() const { return n; }

    // Procesa un bloque in-place. 'display' recibe espectros si no es nulo.
    void process (float* data, int numSamples, const Settings& s, bool wantDisplay)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            inFifo[(size_t) writePos] = data[i];
            writePos = (writePos + 1) % n;

            data[i] = outAcc[(size_t) outReadPos];
            outAcc[(size_t) outReadPos] = 0.0f;
            outReadPos = (outReadPos + 1) % (int) outAcc.size();

            if (++hopCount >= hop)
            {
                hopCount = 0;
                processFrame (s, wantDisplay);
            }
        }
    }

    // Datos para la pantalla
    std::array<float, kDisplayBands> displayIn {}, displayOut {}, displayNoise {};
    float reductionDb = 0.0f;
    bool voiceActive = false;

private:
    void processFrame (const Settings& s, bool wantDisplay)
    {
        // ventana de análisis
        for (int i = 0; i < n; ++i)
            frame[(size_t) i] = { inFifo[(size_t) ((writePos + i) % n)] * window[(size_t) i], 0.0f };
        fft.forward (frame.data());

        ++framesSeen;
        double sigSum = 0.0, noiseSum = 0.0;

        for (int k = 0; k < bins; ++k)
        {
            const float p = std::norm (frame[(size_t) k]) + 1.0e-12f;
            power[(size_t) k] = p;
            float& sp = smoothP[(size_t) k];
            sp = framesSeen == 1 ? p : 0.75f * sp + 0.25f * p;

            float& nz = noise[(size_t) k];
            if (framesSeen <= 1)
                nz = sp;
            else if (sp < nz)
                nz = 0.85f * nz + 0.15f * sp;      // baja rápido hacia el mínimo
            else
                nz = std::min (nz * noiseRise, sp); // sube despacio

            if (k >= voiceLo && k <= voiceHi)
            {
                sigSum += p;
                noiseSum += nz;
            }
        }

        // ¿hay voz? (SNR en la banda vocal)
        const double frameSnr = sigSum / std::max (1.0e-12, noiseSum);
        voiceActive = frameSnr > 4.0;

        const float clarity = (float) std::clamp (s.clarity, 0.0, 1.0);
        const float keep = (float) std::clamp (s.ambience, 0.0, 1.0) * 0.5f;   // hasta -6 dB de ambiente conservado

        for (int k = 0; k < bins; ++k)
        {
            const float p = power[(size_t) k];
            const float nz = std::max (noise[(size_t) k] * 2.2f, 1.0e-12f);   // corrección del sesgo del seguidor de mínimos
            const auto& bw = bandWeights[(size_t) k];
            const float bandAmt = (float) (bw[0] * s.bands[0] + bw[1] * s.bands[1] + bw[2] * s.bands[2] + bw[3] * s.bands[3]);
            const float amt = std::clamp (clarity * bandAmt, 0.0f, 2.0f);

            // SNR a priori (decision-directed)
            const float gammaPost = p / nz;
            const float xi = 0.98f * prevClean[(size_t) k] / nz + 0.02f * std::max (gammaPost - 1.0f, 0.0f);
            const float over = 1.0f + 1.0f * amt;          // sobre-sustracción
            const float xiEff = xi / over;
            float g = xiEff / (1.0f + xiEff);
            // la cantidad define la reducción máxima: 0 = sin efecto, 1 = -45 dB, 2 = -60 dB
            const float floorDb = std::min (60.0f, amt * 45.0f);
            g = std::max (g, std::pow (10.0f, -floorDb / 20.0f));
            g = std::max (g, keep);
            gainTmp[(size_t) k] = g;

            prevClean[(size_t) k] = g * g * p;
        }

        // suavizado en frecuencia (3 bins) y en tiempo
        for (int k = 0; k < bins; ++k)
        {
            const float l = gainTmp[(size_t) std::max (0, k - 1)];
            const float c = gainTmp[(size_t) k];
            const float r = gainTmp[(size_t) std::min (bins - 1, k + 1)];
            const float g = 0.25f * l + 0.5f * c + 0.25f * r;
            float& gs = gainSm[(size_t) k];
            gs += (g - gs) * (g > gs ? 0.7f : 0.35f);
        }

        // compuerta entre frases
        const float gateTarget = (voiceActive || s.gateDepthDb <= 0.0) ? 1.0f
                                 : (float) std::pow (10.0, -s.gateDepthDb / 20.0);
        gateGain += (gateTarget - gateGain) * (gateTarget > gateGain ? gateAttack : gateRelease);

        double inE = 0.0, outE = 0.0;
        for (int k = 0; k < bins; ++k)
        {
            float g = gainSm[(size_t) k];
            if (s.ambienceMode)
                g = 1.0f - g;                // conservar lo que se quitaría
            else
                g *= gateGain;

            if (k >= voiceLo && k <= voiceHi)
            {
                inE += power[(size_t) k];
                outE += power[(size_t) k] * g * g;
            }

            frame[(size_t) k] *= g;
            if (k > 0 && k < n / 2)
                frame[(size_t) (n - k)] = std::conj (frame[(size_t) k]);
        }
        reductionDb = (float) (10.0 * std::log10 (std::max (1.0e-12, outE) / std::max (1.0e-12, inE)));

        if (wantDisplay)
            fillDisplay();

        fft.inverse (frame.data());

        // síntesis: ventana + traslape-suma (suma de Hann al 75 % = 2)
        const int size = (int) outAcc.size();
        for (int i = 0; i < n; ++i)
        {
            const int idx = (outReadPos + i) % size;
            outAcc[(size_t) idx] += frame[(size_t) i].real() * window[(size_t) i] * 0.5f;
        }
    }

    void fillDisplay()
    {
        // 48 bandas logarítmicas de 30 Hz a 18 kHz
        for (int b = 0; b < kDisplayBands; ++b)
        {
            const double f0 = 30.0 * std::pow (600.0, (double) b / kDisplayBands);
            const double f1 = 30.0 * std::pow (600.0, (double) (b + 1) / kDisplayBands);
            int k0 = std::max (1, (int) (f0 * n / sr));
            int k1 = std::min (bins - 1, std::max (k0, (int) (f1 * n / sr)));
            double pi = 0, po = 0, pn = 0;
            for (int k = k0; k <= k1; ++k)
            {
                pi += power[(size_t) k];
                po += std::norm (frame[(size_t) k]);
                pn += noise[(size_t) k];
            }
            const double cnt = k1 - k0 + 1;
            const double norm = 4.0 / ((double) n * n);
            displayIn[(size_t) b] = (float) (10.0 * std::log10 (pi / cnt * norm + 1.0e-12));
            displayOut[(size_t) b] = (float) (10.0 * std::log10 (po / cnt * norm + 1.0e-12));
            displayNoise[(size_t) b] = (float) (10.0 * std::log10 (pn / cnt * norm + 1.0e-12));
        }
    }

    int n = 1024, hop = 256, bins = 513;
    double sr = 44100.0;
    std::vector<float> inFifo, outAcc, window, noise, smoothP, prevClean, gainSm, gainTmp, power;
    std::vector<std::complex<float>> frame;
    std::vector<std::array<float, 4>> bandWeights;
    FFT fft;
    int writePos = 0, hopCount = 0, outReadPos = 0;
    long framesSeen = 0;
    float noiseRise = 1.0f, gateGain = 1.0f, gateAttack = 0.5f, gateRelease = 0.1f;
    int voiceLo = 5, voiceHi = 90;
};

// ----------------------------------------------------------------------------
//  Motor completo (multicanal)
// ----------------------------------------------------------------------------
class VocalCleanEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fftSize = sampleRate > 50000.0 ? 2048 : 1024;
        channels.resize ((size_t) std::max (1, numChannels));
        for (auto& c : channels)
            c.prepare (fftSize, sampleRate);
    }

    int latency() const { return fftSize; }

    void process (float* const* data, int numChannels, int numSamples, const Settings& s)
    {
        const int nCh = std::min (numChannels, (int) channels.size());
        for (int c = 0; c < nCh; ++c)
            channels[(size_t) c].process (data[c], numSamples, s, c == 0);
    }

    const ChannelProcessor& display() const { return channels.front(); }

private:
    int fftSize = 1024;
    std::vector<ChannelProcessor> channels;
};

} // namespace gabyvx
