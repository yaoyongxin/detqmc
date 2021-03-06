/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  See the enclosed file LICENSE for a copy or if
 * that was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2017 Max H. Gerlach
 * 
 * */

#if defined (MAX_DEBUG) && ! defined(DUMA_NO_DUMA)
#include "dumapp.h"
#endif

// Concatenate time series generated by detqmc*.  Pass directories
// with different settings of simulation paramter "simindex"
// containing timeseries files as command line arguments.
//
// Hack: allow to ignore equal "simindex" values, useful for joining
// data from different simulation prefix runs

#include <iostream>
#include <algorithm>            // all_of
#include <iterator>
#include <memory>
#include <map>
#include <cmath>
#include <vector>
#include <string>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#pragma GCC diagnostic pop
#include "git-revision.h"
#include "tools.h"                      //glob
#include "dataseriesloader.h"
#include "dataserieswritersucc.h"
#include "metadata.h"
#include "exceptions.h"


int main(int argc, char **argv) {
    bool ignoreSimindex = false;
    uint32_t discard = 0;
    uint32_t read = 0;
    uint32_t subsample = 1;
    std::vector< std::string > inputDirectories;
    std::string outputDirectory;

    //parse command line options
    namespace po = boost::program_options;
    po::options_description evalOptions("Time series evaluation options");
    evalOptions.add_options()
        ("help", "print help on allowed options and exit")
        ("version,v", "print version information (git hash, build date) and exit")
        ("discard,d", po::value<uint32_t>(&discard)->default_value(0),
         "number of initial time series entries to discard (additional thermalization)")
        ("read,r", po::value<uint32_t>(&read)->default_value(0),
         "maximum number of time series entries to read (after discarded initial samples, before subsampling).  Default value of 0: read all entries")
        ("subsample,s", po::value<uint32_t>(&subsample)->default_value(1),
         "take only every s'th sample into account")
        ("outputDirectory", po::value<std::string>(&outputDirectory)->default_value("."))
        ("inputDirectories", po::value< std::vector<std::string> >(),
         "directories containing timeseries [positional arguments]")
        ("ignoreSimindex", "Ignore data where the same simindex appears multiple times and join the time series anyway.")
        ;
    
    po::positional_options_description positionalArguments; // specify which options are positional
    positionalArguments.add("inputDirectories", -1);        // -1: unlimited
    
    po::variables_map vm;
    //po::store(po::parse_command_line(argc, argv, evalOptions), vm);
    po::store(po::command_line_parser(argc, argv)
              .options(evalOptions)
              .positional(positionalArguments).run(), vm);
    po::notify(vm);

    bool earlyExit = false;
    if (vm.count("help")) {
        std::cout << "Concatenate time series generated by detqmc*.  Pass directories \n"
                  << "with different settings of simulation paramter \"simindex\"\n"
                  << "containing timeseries files as command line arguments.\n"
                  << "Will write resulting time series files into given output directory.\n\n"
                  << evalOptions << std::endl;
        earlyExit = true;
    }
    if (vm.count("version")) {
        std::cout << "Build info:\n"
                  << metadataToString(collectVersionInfo())
                  << std::endl;
        earlyExit = true;
    }
    if (earlyExit) {
        return 0;
    }

    if (vm.count("ignoreSimindex")) {
        ignoreSimindex = true;
    }

    uint32_t dircount = 0;
    if (vm.count("inputDirectories")) {
        inputDirectories = vm["inputDirectories"].as<std::vector< std::string > >();
        dircount = (uint32_t)inputDirectories.size();
    }
    if (dircount == 0) {
        throw_GeneralError("Need to pass at least one input directory (more would make more sense)");
    }

    namespace fs = boost::filesystem;
    fs::path outputDirectory_path(outputDirectory);
    std::vector<fs::path> inputDirectories_path;
    for (const std::string& dir : inputDirectories) {
        inputDirectories_path.push_back(fs::path(dir));
    }

    std::map<uint32_t, uint32_t> simindex_samples;

    // simulation meta data for each simindex, taken from info.dat file
    std::map<uint32_t, MetadataMap> simindex_meta;

    // time series for each observable (filename, which is the same for each subdirecgtory) and each simindex
    std::map<std::string, std::map<uint32_t, std::shared_ptr<std::vector<double> > > > filename_simindex_timeseries;
    // map filename to observable name
    std::map<std::string, std::string> filename_obsname;

    

    std::vector<fs::path> skipped_inputDirectories; // if data is corrupt in one input directory: skip that one
    // process one input directory of time series after the other
    uint32_t in_counter = 0;
    for (fs::path in_path : inputDirectories_path) {
        std::string info_dat_fname = (in_path / fs::path("info.dat")).string();

        //take simulation metadata from subdirectory file info.dat, remove some unnecessary parts.
        //this tells us the simindex.
        MetadataMap this_meta = readOnlyMetadata(info_dat_fname);
        uint32_t this_simindex = 0; // default simindex is 0
        
        // sometimes we just ignore the simindex setting from info.dat
        // and just set it to our own counter
        if (ignoreSimindex) {
            this_simindex = in_counter;
            this_meta["simindex"] = numToString(this_simindex);
        }
        ++in_counter;

        if (this_meta.count("simindex")) {
            this_simindex = fromString<uint32_t>(this_meta["simindex"]);
        } else {
            this_meta["simindex"] = "0";
        }
        if (simindex_samples.count(this_simindex)) {
            throw_GeneralError("simindex " + numToString(this_simindex) + " appears more than one time");
        }
        std::string keys[] = {"buildDate", "buildHost", "buildTime",
                              "cppflags", "cxxflags", "gitBranch", "gitRevisionHash",
                              "sweepsDone", "sweepsDoneThermalization", "totalWallTimeSecs"};
        for ( std::string key : keys) {
            if (this_meta.count(key)) {
                this_meta.erase(key);
            }
        }
        
        uint32_t guessedLength = static_cast<uint32_t>(fromString<double>(this_meta.at("sweeps")) /
                                                       fromString<double>(this_meta.at("measureInterval")));

        // read in time series files -- if necessary, discard or subsample data;
        // if we notice corruption, skip this input directory
        bool corrupt = false;
        std::vector<std::string> filenames = glob((in_path / fs::path("*.series")).string());
        for (std::string fn : filenames) {
            std::cout << "Processing " << fn << ", ";
            DoubleSeriesLoader reader;
            reader.readFromFile(fn, subsample, discard, read, guessedLength);
            if (reader.getColumns() != 1) {
                throw_GeneralError("File " + fn + " does not have exactly 1 column");
            }

            std::shared_ptr<std::vector<double>> data = reader.getData();
            std::string obsName;
            reader.getMeta("observable", obsName);
            std::cout << "observable: " << obsName << "..." << std::flush;

            uint32_t samples = static_cast<uint32_t>(data->size());
            if (simindex_samples.count(this_simindex)) {
                if (samples != simindex_samples[this_simindex]) {
                    std::cerr << "Sample count mismatch in file " << fn << "\n"
                              << "Skipping this directory.\n";
                    corrupt = true;
                    break;
                }
            }
            simindex_samples[this_simindex] = samples;

            std::string bare_filename = fs::path(fn).filename().string();
            filename_simindex_timeseries[bare_filename][this_simindex] = data;
            if (not filename_obsname.count(bare_filename)) {
                filename_obsname[bare_filename] = obsName;
            }
            std::cout << std::endl;
        }

        if (corrupt) {
            skipped_inputDirectories.push_back(in_path);
            continue;
        }
        
        simindex_meta[this_simindex] = this_meta;
    }

    // corrupt data will not be taken into consideration below
    
    
    // compute joined number of samples
    uint32_t total_samples = 0;
    for (const auto& si_samples_pair : simindex_samples) {
        total_samples += si_samples_pair.second;
    }
        
    // reduce simindex_meta to a common metadata map, to do this we want iterators
    // over the values of simindex_meta
    
    //// This nice code does not work with the Intel compiler 15.0, probably due to
    //// a bug in their decltype implementation.
    // auto get_second = [](const std::pair<uint32_t, MetadataMap>& p) -> MetadataMap { return p.second; };
    // auto beg = boost::make_transform_iterator(simindex_meta.begin(), get_second);
    // auto end = boost::make_transform_iterator(simindex_meta.end(), get_second);
    
    // There will not be a huge number of simindex values, so it is
    // not expensive to just create a vector of MetadataMap instances
    std::vector<MetadataMap> temp_vector;
    for (const auto& si_metadatamap_pair : simindex_meta) {
        temp_vector.push_back(si_metadatamap_pair.second);
    }
    auto beg = temp_vector.begin();
    auto end = temp_vector.end();
    MetadataMap common_meta = getCommonMetadata(beg, end);
    common_meta["simindex"] = std::string("joined");    
    common_meta["join-discard"] = numToString(discard);
    common_meta["join-read"] = numToString(read);
    common_meta["join-subsample"] = numToString(subsample);
    common_meta["join-total-samples"] = numToString(total_samples);

    // write out common metadata to info.dat in outputDirectory
    writeOnlyMetaData((outputDirectory_path / fs::path("info.dat")).string(),
                      common_meta);

    // concatenate time series and write them to outputDirectory
    for (const auto& filename_map_pair : filename_simindex_timeseries) {
        const std::string& filename = filename_map_pair.first;
        const std::map<uint32_t, std::shared_ptr<std::vector<double> > >& simindex_timeseries_map = filename_map_pair.second;

        std::vector<double> joined_timeseries;
        for (const auto& simindex_timeseries_pair : simindex_timeseries_map) { // iteration is ordered by simindex 
            std::shared_ptr<std::vector<double> > ts_ptr = simindex_timeseries_pair.second;
            joined_timeseries.insert(joined_timeseries.end(), ts_ptr->begin(), ts_ptr->end());
        }

        MetadataMap output_meta = common_meta;
        output_meta["observable"] = filename_obsname[filename];

        std::string output_filename = (outputDirectory_path / fs::path(filename)).string();
        DoubleVectorWriterSuccessive tsWriter(output_filename);

        tsWriter.addMetadataMap(output_meta);
        tsWriter.addHeaderText("Time series joined by jointimeseries");
        tsWriter.writeHeader();
        tsWriter.writeData(joined_timeseries);        
    }
    
    std::cout << "Done!" << std::endl;

    return 0;
}
