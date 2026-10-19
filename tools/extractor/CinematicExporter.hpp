#ifndef CINEMATIC_EXPORTER_HPP_
#define CINEMATIC_EXPORTER_HPP_

#include <cstdint>
#include <string>

#include "BigfileIndex.hpp"

// Walks every record of bigfile.x64.dat and writes the English text of TextEntry records and of
// CR-delimited language blocks into output_dir (see common/CinematicFiles.hpp for the names).
void ExportCinematicText(const uint8_t* data, size_t fileSize, const BigfileIndex& index, const std::string& output_dir);

#endif //CINEMATIC_EXPORTER_HPP_
