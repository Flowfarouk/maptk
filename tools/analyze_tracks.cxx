/*ckwg +29
 * Copyright 2014 by Kitware, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither name of Kitware, Inc. nor the names of any contributors may be used
 *    to endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <exception>
#include <string>
#include <vector>

#include <maptk/algo/image_io.h>
#include <maptk/algo/analyze_tracks.h>
#include <maptk/algo/draw_tracks.h>
#include <maptk/algorithm_plugin_manager.h>
#include <maptk/camera.h>
#include <maptk/camera_map_io.h>
#include <maptk/landmark_map.h>
#include <maptk/landmark_map_io.h>
#include <maptk/camera_io.h>
#include <maptk/projected_track_set.h>
#include <maptk/image_container.h>
#include <maptk/config_block.h>
#include <maptk/config_block_io.h>
#include <maptk/exceptions.h>
#include <maptk/track_set_io.h>
#include <maptk/types.h>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>


namespace bfs = boost::filesystem;


static maptk::config_block_sptr default_config()
{
  maptk::config_block_sptr config =
    maptk::config_block::empty_config( "analyze_tracks_tool" );

  config->set_value( "track_file", "",
                     "Path to a required input file containing all features tracks "
                     "generated from some prior processing." );
  config->set_value( "image_list_file", "",
                     "Path to an optional input file containing new-line separated "
                     "paths to sequential image files for the given tracks. This "
                     "file is required for draw tracks output." );
  config->set_value( "output_file", "",
                     "Path to an optional file to write text outputs to. If this file "
                     "exists, it will be overwritten." );
  config->set_value( "comparison_track_file", "",
                     "Path to an optional second track input file containing tracks "
                     "which we want to compare against the first set." );
  config->set_value( "comparison_landmark_file", "",
                     "Path to an optional landmark ply file, which can be used along "
                     "with a camera file to generate a comparison track set." );
  config->set_value( "comparison_camera_dir", "",
                     "Path to an optional camera directory, which can be used alongside "
                     "a landmark ply file to generate a comparison track set." );

  maptk::algo::analyze_tracks::get_nested_algo_configuration(
    "track_analyzer", config, maptk::algo::analyze_tracks_sptr() );

  return config;
}


static bool check_config( maptk::config_block_sptr config )
{
  if( !config->has_value( "track_file" ) ||
      !bfs::exists( maptk::path_t( config->get_value<std::string>( "track_file" ) ) ) )
  {
    std::cerr << "A valid track file must be specified!" << std::endl;
    return false;
  }

  if( !maptk::algo::analyze_tracks::check_nested_algo_configuration( "track_analyzer", config ) )
  {
    std::cerr << "Invalid analyze_tracks config" << std::endl;
    return false;
  }

  if( config->has_value( "image_list_file" ) &&
      !config->get_value<std::string>( "image_list_file" ).empty() )
  {
    if( !bfs::exists( maptk::path_t( config->get_value<std::string>( "image_list_file" ) ) ) )
    {
      std::cerr << "Cannot find image list file" << std::endl;
      return false;
    }
    else if( !maptk::algo::image_io::check_nested_algo_configuration( "image_reader", config ) ||
             !maptk::algo::image_io::check_nested_algo_configuration( "track_drawer", config ) )
    {
      std::cerr << "Unable to configure track drawer" << std::endl;
      return false;
    }
  }

  if( config->has_value( "comparison_landmark_file" ) !=
      config->has_value( "comparison_camera_dir" ) )
  {
    std::cerr << "Both a landmark and camera file must be specified to use either." << std::endl;
    return false;
  }

  return true;
}


static int maptk_main(int argc, char const* argv[])
{
  // register the algorithm implementations
  maptk::algorithm_plugin_manager::instance().register_plugins();

  // define/parse CLI options
  boost::program_options::options_description opt_desc;
  opt_desc.add_options()
    ( "help,h", "output help message and exit" )
    ( "config,c",
      boost::program_options::value<maptk::path_t>(),
      "Configuration file for the tool." )
    ( "output-config,o",
      boost::program_options::value<maptk::path_t>(),
      "Output a configuration.This may be seeded with a configuration file from -c/--config." )
    ;
  boost::program_options::variables_map vm;

  try
  {
    boost::program_options::store(
      boost::program_options::parse_command_line(argc, argv, opt_desc), vm );
  }
  catch( boost::program_options::unknown_option const& e )
  {
    std::cerr << "Error: unknown option " << e.get_option_name() << std::endl;
    return EXIT_FAILURE;
  }

  boost::program_options::notify( vm );

  if( vm.count( "help" ) )
  {
    std::cerr << opt_desc << std::endl;
    return EXIT_SUCCESS;
  }

  // Set config to algo chain
  // Get config from algo chain after set
  // Check config validity, store result
  //
  // If -o/--output-config given, output config result and notify of current (in)validity
  // Else error if provided config not valid.
  namespace algo = maptk::algo;
  namespace bfs = boost::filesystem;

  // Set up top level configuration w/ defaults where applicable.
  maptk::config_block_sptr config = default_config();

  algo::image_io_sptr image_reader;
  algo::analyze_tracks_sptr analyze_tracks;
  algo::draw_tracks_sptr draw_tracks;

  // If -c/--config given, read in confgi file, merge in with default just generated
  if( vm.count( "config" ) )
  {
    config->merge_config( maptk::read_config_file( vm[ "config" ].as<maptk::path_t>() ) );
  }

  // Load all input images if they are specified
  bool use_images = config->has_value( "image_list_file" ) &&
                    !config->get_value<std::string>( "image_list_file" ).empty();

  bool output_to_file = config->has_value( "output_file" ) &&
                        !config->get_value<std::string>( "output_file" ).empty();

  if( use_images )
  {
    algo::image_io::set_nested_algo_configuration( "image_reader", config, image_reader );
    algo::image_io::get_nested_algo_configuration( "image_reader", config, image_reader );

    algo::draw_tracks::set_nested_algo_configuration( "track_drawer", config, draw_tracks );
    algo::draw_tracks::get_nested_algo_configuration( "track_drawer", config, draw_tracks );
  }

  algo::analyze_tracks::set_nested_algo_configuration( "track_analyzer", config, analyze_tracks );
  algo::analyze_tracks::get_nested_algo_configuration( "track_analyzer", config, analyze_tracks );

  bool valid_config = check_config( config );

  // Output a config file if specified
  if( vm.count( "output-config" ) )
  {
    write_config_file( config, vm[ "output-config" ].as<maptk::path_t>() );

    if( valid_config )
    {
      std::cerr << "INFO: Configuration file contained valid parameters and may be used "
                << "for running" << std::endl;
    }
    else
    {
      std::cerr << "WARNING: Configuration deemed not valid for running." << std::endl;
    }
    return EXIT_SUCCESS;
  }
  else if( !valid_config )
  {
    std::cerr << "ERROR: Configuration not valid." << std::endl;
    return EXIT_FAILURE;
  }

  // Load main track set
  maptk::track_set_sptr tracks;

  std::cout << std::endl << "Loading main track set file..." << std::endl;
  std::string track_file = config->get_value<std::string>( "track_file" );
  tracks = maptk::read_track_file( track_file );

  // Generate statistics if enabled
  if( analyze_tracks )
  {
    std::cout << std::endl << "Generating track statistics..." << std::endl;

    if( output_to_file )
    {
      std::string output_file = config->get_value<std::string>( "output_file" );
      std::ofstream ofs( output_file.c_str() );

      if( !ofs )
      {
        std::cerr << "Error: Could not open file " << output_file << " for writing." << std::endl;
        return EXIT_FAILURE;
      }

      analyze_tracks->print_info( tracks, ofs );

      ofs.close();
    }
    else
    {
      analyze_tracks->print_info( tracks, std::cout );
    }
  }

  // Read and process input images if set
  if( use_images )
  {
    std::vector<maptk::path_t> image_paths;
    std::string image_list_file = config->get_value<std::string>( "image_list_file" );
    std::ifstream ifs( image_list_file.c_str() );

    if( !ifs )
    {
      std::cerr << "Error: Could not open image list \"" << image_list_file << "\"" << std::endl;
      return EXIT_FAILURE;
    }

    // Creating input image list, checking file existance, and loading the image
    for( std::string line; std::getline(ifs,line); )
    {
      image_paths.push_back( line );
    }

    // Load comparison tracks if enabled
    maptk::track_set_sptr comparison_tracks;

    if( config->has_value( "comparison_track_file" ) &&
        !config->get_value<std::string>( "comparison_track_file" ).empty() )
    {
      track_file = config->get_value<std::string>( "comparison_track_file" );

      std::cout << std::endl << "Loading comparison track set file..." << std::endl;

      comparison_tracks = maptk::read_track_file( track_file );
    }
    else if( config->has_value( "comparison_landmark_file" ) &&
             !config->get_value<std::string>( "comparison_landmark_file" ).empty() &&
             config->has_value( "comparison_camera_dir" ) &&
             !config->get_value<std::string>( "comparison_camera_dir" ).empty() )
    {
      std::string landmark_file = config->get_value<std::string>( "comparison_landmark_file" );
      std::string camera_dir = config->get_value<std::string>( "comparison_camera_dir" );

      std::cout << std::endl << "Loading comparison track set file..." << std::endl;

      maptk::landmark_map_sptr landmarks = maptk::read_ply_file( landmark_file );
      maptk::camera_map_sptr cameras = maptk::read_krtd_files( image_paths, camera_dir );

      if( !cameras )
      {
        std::cerr << "Unable to load any camera files." << std::endl;
        return EXIT_FAILURE;
      }

      if( !landmarks )
      {
        std::cerr << "Unable to load landmark file." << std::endl;
        return EXIT_FAILURE;
      }

      comparison_tracks = projected_tracks( landmarks, cameras );
    }

    // Read images one by one, this is more memory efficient than loading them all
    std::cout << std::endl << "Generating feature images..." << std::endl;

    for( unsigned i = 0; i < image_paths.size(); i++ )
    {
      if( !bfs::exists( image_paths[i] ) )
      {
        throw maptk::path_not_exists( image_paths[i] );
      }

      maptk::image_container_sptr_list images;

      maptk::image_container_sptr image = image_reader->load( image_paths[i].string() );
      images.push_back( image );

      // Draw tracks on images
      draw_tracks->draw( tracks, images, comparison_tracks );
    }
  }

  std::cout << std::endl;
  return EXIT_SUCCESS;
}


int main( int argc, char const* argv[] )
{
  try
  {
    return maptk_main( argc, argv );
  }
  catch( std::exception const& e )
  {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch( ... )
  {
    std::cerr << "Unknown exception caught" << std::endl;
    return EXIT_FAILURE;
  }
}
