cmake_minimum_required(VERSION 3.22)

project(DevKomodoPedals VERSION 1.0.0)

# Descarga JUCE UNA sola vez; todos los pedales de abajo la comparten.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Cada pedal es un subproyecto independiente con su propio target/plugin.
add_subdirectory(Pedals/Overdrive)
add_subdirectory(Pedals/Chorus)
add_subdirectory(Pedals/Reverb)
add_subdirectory(Pedals/SynthGuitar)
add_subdirectory(Pedals/SynthBass)
add_subdirectory(Pedals/FuzzGuitar)
add_subdirectory(Pedals/FuzzBass)
add_subdirectory(Pedals/DistortionGuitar)
add_subdirectory(Pedals/DistortionBass)
add_subdirectory(Pedals/Compressor)
add_subdirectory(Pedals/Tremolo)
add_subdirectory(Pedals/Phaser)
add_subdirectory(Pedals/Delay)
add_subdirectory(Pedals/NoiseGate)
add_subdirectory(Pedals/Boost)
add_subdirectory(Pedals/RingModulator)
add_subdirectory(Pedals/EQ)
add_subdirectory(Pedals/OverdriveBass)
add_subdirectory(Pedals/ChorusBass)
add_subdirectory(Pedals/ReverbBass)
add_subdirectory(Pedals/PhaserBass)
