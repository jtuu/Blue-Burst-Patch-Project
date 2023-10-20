#ifdef PATCH_MUSIC_VISUALIZER

#include "common.h"
#include "helpers.h"
#include "keyboard.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

void PatchInfiniteLobbyBurst()
{
    memset((void *) 0x007bdad4, 0x90, 4);
    *(uint8_t *) 0x007bdb3a = 0xeb; // JMP
    *(uint8_t*) 0x004188d5 = 0xeb; // Remove cursor
}

#include <fftw3.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <ctime>

const size_t intensities_count = 36;
double particle_intensities[intensities_count] = {0.0};
double intensity_sum = 0.0;

double Length(fftw_complex c) {
    return sqrt(c[0] * c[0] + c[1] * c[1]);
}

double linear_scale(double n, double src_min, double src_max, double dst_min, double dst_max) {
    return (n - src_min) / (src_max - src_min) * (dst_max - dst_min) + dst_min;
}

int iter = 0;
double clamp_point = 3.0;
double amplification = 50.0;
double sc_clamp = 0.7;
double sc_dampen = 25.0;

void AudioCallback(void* ud, Uint8* stream, int len)
{
    std::fill(std::begin(particle_intensities), std::end(particle_intensities), 0.0);
    intensity_sum = 0.0;

    double* stream_real = fftw_alloc_real(len);
    for (int i = 0; i < len; i++) {
        stream_real[i] = ((double)stream[i]);
    }

    size_t spectrum_len = len / 2 + 1;
    fftw_complex* spectrum = fftw_alloc_complex(spectrum_len);
    fftw_plan plan = fftw_plan_dft_r2c_1d(len, stream_real, spectrum, 0);
    fftw_execute(plan);

    for (size_t i = 0; i < spectrum_len; i++) {
        intensity_sum += log10(Length(spectrum[i]));
    }

    for (size_t i = 0; i < intensities_count; i++) {
        size_t i_lo = i * spectrum_len / intensities_count;
        size_t i_hi = (i + 1) * spectrum_len / intensities_count;
        double bin_strength = 1.0 / (double) (i_hi - i_lo);

        for (size_t j = i_lo; j < i_hi; j++) {
            double amp = log10(Length(spectrum[j]));
            if (++iter % 100 == 0) {
                //Log(ToWideString(std::to_string(amp)).c_str());
            }
            if (amp > clamp_point) {
                amp = amp - clamp_point;
            } else {
                continue;
            }
            amp = linear_scale(amp, 0.0, 1.5, 0.0, (double) intensities_count);

            size_t amp_bin = (size_t) amp;
            if (amp_bin < 0 || amp_bin >= intensities_count) {
                continue;
            }
            
            particle_intensities[amp_bin] += bin_strength;
        }
    }

    if (iter % 100 == 0) {
        std::string s = "";
        for (int i = 0; i < intensities_count; i++) {
            s += std::to_string(particle_intensities[i]) + " ";
        }
        //Log(ToWideString(s).c_str());
    }

    fftw_destroy_plan(plan);
    fftw_free(stream_real);
    fftw_free(spectrum);
}

void __cdecl BeforeBurstBegin(int arg)
{
    ((decltype(BeforeBurstBegin) *) 0x00417cb8)(arg); // Orig

    SDL_Init(SDL_INIT_AUDIO);
    Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 512 * 2);
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    Mix_Music* music = Mix_LoadMUS("satellite.mp3");
    Mix_SetPostMix(AudioCallback, NULL);
    Mix_PlayMusic(music, 1000);
}

double sum_scale = 7500.0;

void __fastcall BeforeApplyParticleVelocity(float* position, float* velocity)
{
    float common_range = 0.1;
    float sum_range = sum_scale * (1 - common_range);
    if (intensity_sum > sum_range) {
        float brightness = 1.0 - linear_scale(intensity_sum - sum_range, 0.0, sum_scale * common_range, 0.0, 0.1);
        *(float*) 0x0092043c = brightness;
    }

    double pi = 3.14159265359;
    double ang;
    bool is_towards = velocity[0] == 0.0 && velocity[1] == 0.0 && velocity[2] != 0.0;
    bool is_center = position[0] == 0.0 && position[1] == 0.0;
    if (is_towards) {
        ang = atan2(position[1], position[0]);
    } else {
        ang = atan2(velocity[1], velocity[0]);
    }
    size_t i = (size_t) ((ang + pi) / (pi * 4.0) * (double) intensities_count); // XXX: Why does pi/4 look better?
    double intensity = particle_intensities[i];
    if (intensity < sc_clamp) {
        intensity = 0.0;
    } else if (intensity != 0.0) {
        intensity += 1.0;
        intensity = pow(intensity, 3.0) / sc_dampen;
    }
    if (is_towards) {
        if (!is_center) {
            velocity[2] *= 0.5 + linear_scale(intensity_sum - sum_scale, 0.0, sum_scale, 0.0, 1.0);
        }
    } else {
        velocity[0] *= intensity;
        velocity[1] *= intensity;
        if (velocity[2] && intensity == 0.0) {
            velocity[2] = 0.0;
        }
    }
    if (++iter % 100 == 0) {
        //Log(ToWideString(std::to_string(ang) + " " + std::to_string(i) + " " + std::to_string(intensity) + " " + std::to_string(position[0]) + "," + std::to_string(position[1])).c_str());
        if (position[0] == 0.0 && position[1] == 0.0) {
            Log(ToWideString(std::to_string(position[2])).c_str());
        }
        //Log(ToWideString(std::to_string(intensity_sum)).c_str());
    }
    ((decltype(BeforeApplyParticleVelocity) *) 0x0082ebac)(position, velocity);
}

void __thiscall BeforeCullStagnantParticle(size_t arg) {
    float* scale_x = (float *) (arg + 0x2c);
    float* scale_y = (float *) (arg + 0x30);
    float common_range = 0.1;
    double range = sum_scale * (1 - common_range);
    if (*(float*)(arg + 0x74) == 0.0 && *(float*)(arg + 0x78) == 0.0 && *(float*)(arg + 0x7c) == -100.0 && intensity_sum > range) {
        float intensity = intensity_sum - range;
        float scale = 0.0 + linear_scale(intensity, 0.0, sum_scale * common_range, 0.0, 2.0);
        *scale_x = scale;
        *scale_y = scale;
    }

    ((decltype(BeforeCullStagnantParticle) *) 0x0050e618)(arg);
}

void __cdecl DuringBurstUpdate()
{
    if (Keyboard::wasPressed(Keyboard::Keycode::Q)) {
        clamp_point -= 0.1;
        Log(ToWideString("clamp point: " + std::to_string(clamp_point)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::W)) {
        clamp_point += 0.1;
        Log(ToWideString("clamp point: " + std::to_string(clamp_point)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::E)) {
        amplification -= 1.0;
        Log(ToWideString("amplification: " + std::to_string(amplification)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::R)) {
        amplification += 1.0;
        Log(ToWideString("amplification: " + std::to_string(amplification)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::T)) {
        sc_clamp -= 0.1;
        Log(ToWideString("sc clamp: " + std::to_string(sc_clamp)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::Y)) {
        sc_clamp += 0.1;
        Log(ToWideString("sc clamp: " + std::to_string(sc_clamp)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::U)) {
        sc_dampen -= 1.0;
        Log(ToWideString("sc dampen: " + std::to_string(sc_dampen)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::I)) {
        sc_dampen += 1.0;
        Log(ToWideString("sc dampen: " + std::to_string(sc_dampen)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::O)) {
        sum_scale -= 10.0;
        Log(ToWideString("brightness_scale: " + std::to_string(sum_scale)).c_str());
    }

    if (Keyboard::wasPressed(Keyboard::Keycode::P)) {
        sum_scale += 10.0;
        Log(ToWideString("brightness_scale: " + std::to_string(sum_scale)).c_str());
    }
}

void PatchParticleVelocity()
{
    PatchCALL(0x007bdac7, 0x007bdac7 + 5, (int) BeforeBurstBegin);
    PatchCALL(0x0050dd36, 0x0050dd36 + 5, (int) BeforeApplyParticleVelocity);
    PatchCALL(0x007bdaec, 0x007bdaec + 5, (int) DuringBurstUpdate);
    PatchCALL(0x0050e437, 0x0050e437 + 5, (int) BeforeCullStagnantParticle);
}

void ApplyMusicVisualizerPatch()
{
    /*
    target_link_libraries(${PROJECT_NAME} fftw3)
    target_link_libraries(${PROJECT_NAME} SDL2)
    target_link_libraries(${PROJECT_NAME} SDL2_mixer)
    */
    
    PatchInfiniteLobbyBurst();
    PatchParticleVelocity();
}

#endif // PATCH_MUSIC_VISUALIZER
