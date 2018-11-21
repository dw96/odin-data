/*
 * BloscPlugin.cpp
 *
 *  Created on: 22 Jan 2018
 *      Author: Ulrik Pedersen
 */
#include <cstdlib>
#include <blosc.h>
#include <version.h>
#include <BloscPlugin.h>
#include <DebugLevelLogger.h>

namespace FrameProcessor
{

const std::string BloscPlugin::CONFIG_BLOSC_COMPRESSOR = "compressor";
const std::string BloscPlugin::CONFIG_BLOSC_THREADS    = "threads";
const std::string BloscPlugin::CONFIG_BLOSC_LEVEL      = "level";
const std::string BloscPlugin::CONFIG_BLOSC_SHUFFLE    = "shuffle";


  /**
 * cd_values[7] meaning (see blosc.h):
 *   0: reserved
 *   1: reserved
 *   2: type size
 *   3: uncompressed size
 *   4: compression level
 *   5: 0: shuffle not active, 1: byte shuffle, 2: bit shuffle
 *   6: the actual Blosc compressor to use. See blosc.h
 *
 * @param settings
 * @return
 */
void create_cd_values(const BloscCompressionSettings& settings, std::vector<unsigned int>& cd_values)
{
  if (cd_values.size() < 7) cd_values.resize(7);
  cd_values[0] = 0;
  cd_values[1] = 0;
  cd_values[2] = static_cast<unsigned int>(settings.type_size);
  cd_values[3] = static_cast<unsigned int>(settings.uncompressed_size);
  cd_values[4] = settings.compression_level;
  cd_values[5] = settings.shuffle;
  cd_values[6] = settings.blosc_compressor;
}

/**
* The constructor sets up logging used within the class.
*/
BloscPlugin::BloscPlugin() :
current_acquisition_("")
{
  this->commanded_compression_settings_.blosc_compressor = BLOSC_LZ4;
  this->commanded_compression_settings_.shuffle = BLOSC_BITSHUFFLE;
  this->commanded_compression_settings_.compression_level = 1;
  this->commanded_compression_settings_.type_size = 0;
  this->commanded_compression_settings_.uncompressed_size = 0;
  this->commanded_compression_settings_.threads = 1;
  this->compression_settings_ = this->commanded_compression_settings_;

  // Setup logging for the class
  logger_ = Logger::getLogger("FP.BloscPlugin");
  logger_->setLevel(Level::getAll());
  LOG4CXX_TRACE(logger_, "BloscPlugin constructor. Version: " << this->get_version_long());

  //blosc_init(); // not required for blosc >= v1.9
  int ret = 0;
  ret = blosc_set_compressor(BLOSC_LZ4_COMPNAME);
  if (ret < 0) LOG4CXX_ERROR(logger_, "Blosc unable to set compressor: " << BLOSC_LZ4_COMPNAME);
  blosc_set_nthreads(this->compression_settings_.threads);
  LOG4CXX_TRACE(logger_, "Blosc Version: " << blosc_get_version_string());
  LOG4CXX_TRACE(logger_, "Blosc list available compressors: " << blosc_list_compressors());
  LOG4CXX_TRACE(logger_, "Blosc current compressor: " << blosc_get_compressor());
}

/**
 * Destructor.
 */
BloscPlugin::~BloscPlugin()
{
  LOG4CXX_DEBUG_LEVEL(3, logger_, "BloscPlugin destructor.");
}

/**
 * Compress one frame, return compressed frame.
 */
boost::shared_ptr<Frame> BloscPlugin::compress_frame(boost::shared_ptr<Frame> src_frame)
{
  int compressed_size = 0;
  BloscCompressionSettings c_settings;
  boost::shared_ptr <Frame> dest_frame;

  const void* src_data_ptr = static_cast<const void*>(
      static_cast<const char*>(src_frame->get_data())
  );

  this->update_compression_settings(src_frame->get_acquisition_id());
  c_settings = this->compression_settings_;
  if (src_frame->get_data_type() >= 0) {
    c_settings.type_size = src_frame->get_data_type_size();
  }
  else { // TODO: This if/else is a hack to work around Frame::data_type_ not being set. See https://jira.diamond.ac.uk/browse/BC-811
    c_settings.type_size = 2; // hack: just default to 16bit per pixel as Excalibur use that
  }
  c_settings.uncompressed_size = src_frame->get_data_size();

  size_t dest_data_size = c_settings.uncompressed_size + BLOSC_MAX_OVERHEAD;
  // TODO: is this malloc really necessary? Can't we get writable DataBlocks somehow?
  void *dest_data_ptr = malloc(dest_data_size);
  if (dest_data_ptr == NULL) {throw std::runtime_error("Failed to malloc buffer for Blosc compression output");}

  std::stringstream ss_blosc_settings;
  ss_blosc_settings << " compressor=" << blosc_get_compressor()
                    << " threads=" << blosc_get_nthreads()
                    << " clevel=" << c_settings.compression_level
                    << " doshuffle=" << c_settings.shuffle
                    << " typesize=" << c_settings.type_size
                    << " nbytes=" << c_settings.uncompressed_size
                    << " destsize=" << dest_data_size;

  LOG4CXX_DEBUG_LEVEL(2, logger_, "Blosc compression: frame=" << src_frame->get_frame_number()
                          << " acquisition=\"" << src_frame->get_acquisition_id() << "\""
                          << ss_blosc_settings.str()
                          << " src=" << src_data_ptr
                          << " dest=" << dest_data_ptr);
  compressed_size = blosc_compress(c_settings.compression_level, c_settings.shuffle,
                                   c_settings.type_size,
                                   c_settings.uncompressed_size, src_data_ptr,
                                   dest_data_ptr, dest_data_size);
  if (compressed_size < 0) {
    std::stringstream ss;
    ss << "blosc_compress failed. error=" << compressed_size << ss_blosc_settings.str();
    LOG4CXX_ERROR(logger_, ss.str());
    throw std::runtime_error(ss.str());
  }
  double factor = 0.;
  if (compressed_size > 0) {
    factor = (double)src_frame->get_data_size() / (double)compressed_size;
  }
  LOG4CXX_DEBUG_LEVEL(2, logger_, "Blosc compression complete: frame=" << src_frame->get_frame_number()
                                  << " compressed_size=" << compressed_size
                                  << " factor=" << factor);

  dest_frame = boost::shared_ptr<Frame>(new Frame(src_frame->get_dataset_name()));

  LOG4CXX_DEBUG_LEVEL(3, logger_, "Copying compressed data to output frame. (" << compressed_size << " bytes)");
  // I wish we had a pointer swap feature on the Frame class and avoid this unnecessary copy...
  dest_frame->copy_data(dest_data_ptr, compressed_size);
  if (dest_data_ptr != NULL) {free(dest_data_ptr); dest_data_ptr = NULL;}

  // I wish we had a shallow-copy feature on the Frame class...
  dest_frame->set_data_type(src_frame->get_data_type());
  dest_frame->set_frame_number(src_frame->get_frame_number());
  dest_frame->set_acquisition_id(src_frame->get_acquisition_id());
  // TODO: is this the correct way to get and set dimensions?
  dest_frame->set_dimensions(src_frame->get_dimensions());
  return dest_frame;
}

/**
 * Update the compression settings used if the acquisition ID differs from the current one.
 * i.e. if a new acquisition has been started.
 *
 * @param acquisition_id
 */
void BloscPlugin::update_compression_settings(const std::string &acquisition_id)
{
  if (acquisition_id != this->current_acquisition_){
    LOG4CXX_DEBUG_LEVEL(1, logger_, "New acquisition detected: "<< acquisition_id);
    this->compression_settings_ = this->commanded_compression_settings_;
    this->current_acquisition_ = acquisition_id;

    int ret = 0;
    const char * p_compressor_name;
    ret = blosc_compcode_to_compname(this->compression_settings_.blosc_compressor, &p_compressor_name);
    LOG4CXX_DEBUG_LEVEL(1, logger_, "Blosc compression new acquisition=\"" << acquisition_id << "\":"
                                    << " compressor=" << p_compressor_name
                                    << " threads=" << blosc_get_nthreads()
                                    << " clevel=" << this->compression_settings_.compression_level
                                    << " doshuffle=" << this->compression_settings_.shuffle
                                    << " typesize=" << this->compression_settings_.type_size
                                    << " nbytes=" << this->compression_settings_.uncompressed_size);
    ret = blosc_set_compressor(p_compressor_name);
    if (ret < 0) {
      LOG4CXX_ERROR(logger_, "Blosc failed to set compressor: "
          << " " << this->compression_settings_.blosc_compressor
          << " " << *p_compressor_name)
      throw std::runtime_error("Blosc failed to set compressor");
    }
  }
}

  /**
 * Perform compression on the frame and output a new, compressed Frame.
 *
 * \param[in] frame - Pointer to a Frame object.
 */
void BloscPlugin::process_frame(boost::shared_ptr<Frame> src_frame)
{
  // Protect this method
  boost::lock_guard<boost::recursive_mutex> lock(mutex_);

  LOG4CXX_DEBUG_LEVEL(3, logger_, "Received a new frame...");
  boost::shared_ptr <Frame> compressed_frame = this->compress_frame(src_frame);
  LOG4CXX_DEBUG_LEVEL(3, logger_, "Pushing compressed frame");
  this->push(compressed_frame);
}

void BloscPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply)
{
  // Protect this method
  boost::lock_guard<boost::recursive_mutex> lock(mutex_);

  LOG4CXX_INFO(logger_, config.encode());

  if (config.has_param(BloscPlugin::CONFIG_BLOSC_LEVEL)) {
    // NOTE: we don't catch exceptions here because the get_param basically only throws IpcMessagException
    //       if the parameter isn't found - and we've just checked for that in the previous line...
    int blosc_level = config.get_param<int>(BloscPlugin::CONFIG_BLOSC_LEVEL);
    // Range checking: check and cap at upper and lower bounds
    if (blosc_level < 1) {
      this->commanded_compression_settings_.compression_level = 1;
      LOG4CXX_WARN(logger_, "Commanded blosc level: " << blosc_level << "Capped at lower range: 1");
      reply.set_param<std::string>("warning: level", "Capped at lower range: 1");
    } else if(blosc_level > 9) {
      this->commanded_compression_settings_.compression_level = 9;
      LOG4CXX_WARN(logger_, "Commanded blosc level: " << blosc_level << "Capped at upper range: 9");
      reply.set_param<std::string>("warning: level", "Capped at upper range: 9");
    } else {
      this->commanded_compression_settings_.compression_level = blosc_level;
    }
  }

  if (config.has_param(BloscPlugin::CONFIG_BLOSC_SHUFFLE)) {
    unsigned int blosc_shuffle = config.get_param<unsigned int>(BloscPlugin::CONFIG_BLOSC_SHUFFLE);
    // Range checking: 0, 1, 2 are valid values. Anything else result in setting value 0 (no shuffle)
    if (blosc_shuffle > BLOSC_BITSHUFFLE) {
      this->commanded_compression_settings_.shuffle = 0;
      LOG4CXX_WARN(logger_, "Commanded blosc shuffle: " << blosc_shuffle << " is invalid. Disabling SHUFFLE filter");
      reply.set_param<std::string>("warning: shuffle filter", "Disabled");
    } else {
      this->commanded_compression_settings_.shuffle = blosc_shuffle;
    }
  }

  if (config.has_param(BloscPlugin::CONFIG_BLOSC_THREADS)) {
    unsigned int blosc_threads = config.get_param<unsigned int>(BloscPlugin::CONFIG_BLOSC_THREADS);
    if (blosc_threads > BLOSC_MAX_THREADS) {
      this->commanded_compression_settings_.threads = 8;
      LOG4CXX_WARN(logger_, "Commanded blosc threads: " << blosc_threads << " is too large. Setting 8 threads.");
      reply.set_param<int>("warning: threads", 4);
    } else {
      this->commanded_compression_settings_.threads = blosc_threads;
    }
  }

  if (config.has_param(BloscPlugin::CONFIG_BLOSC_COMPRESSOR)) {
    unsigned int blosc_compressor = config.get_param<unsigned int>(BloscPlugin::CONFIG_BLOSC_COMPRESSOR);
    if (blosc_compressor > BLOSC_ZSTD) {
      this->commanded_compression_settings_.blosc_compressor = BLOSC_LZ4;
      LOG4CXX_WARN(logger_, "Commanded blosc compressor: "
                            << blosc_compressor << " is invalid. Setting compressor: "
                            << BLOSC_LZ4 << "(" << BLOSC_LZ4_COMPNAME << ")");
      reply.set_param<int>("warning: compressor", BLOSC_LZ4);
    } else {
      this->commanded_compression_settings_.blosc_compressor = blosc_compressor;
    }
  }
}

void BloscPlugin::requestConfiguration(OdinData::IpcMessage& reply)
{
  reply.set_param(this->get_name() + "/" + BloscPlugin::CONFIG_BLOSC_COMPRESSOR,
                  this->commanded_compression_settings_.blosc_compressor);
  reply.set_param(this->get_name() + "/" + BloscPlugin::CONFIG_BLOSC_THREADS,
                  this->commanded_compression_settings_.threads);
  reply.set_param(this->get_name() + "/" + BloscPlugin::CONFIG_BLOSC_SHUFFLE,
                  this->commanded_compression_settings_.shuffle);
  reply.set_param(this->get_name() + "/" + BloscPlugin::CONFIG_BLOSC_LEVEL,
                  this->commanded_compression_settings_.compression_level);
}

void BloscPlugin::status(OdinData::IpcMessage& status)
{
  status.set_param(this->get_name() + "/compressor", this->compression_settings_.blosc_compressor);
  status.set_param(this->get_name() + "/threads", this->compression_settings_.threads);
  status.set_param(this->get_name() + "/shuffle", this->compression_settings_.shuffle);
  status.set_param(this->get_name() + "/level", this->compression_settings_.compression_level);
}

int BloscPlugin::get_version_major()
{
  return ODIN_DATA_VERSION_MAJOR;
}

int BloscPlugin::get_version_minor()
{
  return ODIN_DATA_VERSION_MINOR;
}

int BloscPlugin::get_version_patch()
{
  return ODIN_DATA_VERSION_PATCH;
}

std::string BloscPlugin::get_version_short()
{
  return ODIN_DATA_VERSION_STR_SHORT;
}

std::string BloscPlugin::get_version_long()
{
  return ODIN_DATA_VERSION_STR;
}

} /* namespace FrameProcessor */
