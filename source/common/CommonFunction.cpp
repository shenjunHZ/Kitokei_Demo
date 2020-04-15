#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include "CommonFunction.hpp"
#include "Configurations/Configurations.hpp"

namespace common
{
    bool isFileExistent(const std::string& fileName)
    {
        const boost::filesystem::path filePathName(fileName);
        boost::system::error_code error;
        auto file_status = boost::filesystem::status(filePathName, error);
        if (error) 
        {
            return false;
        }

        if (not boost::filesystem::exists(file_status)) 
        {
            return false;
        }

        if (boost::filesystem::is_directory(file_status)) 
        {
            return false;
        }

        return true;
    }

    std::string getCaptureOutputDir(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureOutputDir) != config.end())
        {
            return config[configuration::captureOutputDir].as<std::string>();
        }
        return "/tmp/cameraCapture/";
    }
} // namespace common

namespace video
{
    constexpr int V4l2RequestBuffersCounter = 4;

    std::string getVideoName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoName) != config.end())
        {
            return config[configuration::videoName].as<std::string>();
        }
        return "chessVideo";
    }

    std::string getDefaultCameraDevice(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::cameraDevice) != config.end())
        {
            return config[configuration::cameraDevice].as<std::string>();
        }
        return "/dev/video0";
    }

    std::string getDefaultAudioRecord(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioRecord) != config.end())
        {
            return config[configuration::audioRecord].as<std::string>();
        }
        return "plughw:1,0";
    }

    bool getEnableCameraStream(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::enableCameraStream) != config.end())
        {
            return config[configuration::enableCameraStream].as<bool>();
        }
        return true;
    }

    int getV4l2RequestBuffersCounter(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::V4l2RequestBuffersCounter) != config.end())
        {
            return config[configuration::V4l2RequestBuffersCounter].as<int>();
        }
        return V4l2RequestBuffersCounter;
    }

    std::string getV4L2CaptureFormat(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::V4L2CaptureFormat) != config.end())
        {
            return config[configuration::V4L2CaptureFormat].as<std::string>();
        }
        return "BMP";
    }

    std::string getPipeFileName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::pipeFileName) != config.end())
        {
            return config[configuration::pipeFileName].as<std::string>();
        }
        return "cameraCapturePipe";
    }

    int getCaptureWidth(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureWidth) != config.end())
        {
            return config[configuration::captureWidth].as<int>();
        }
        return 640;
    }

    int getCaptureHeight(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureHeight) != config.end())
        {
            return config[configuration::captureHeight].as<int>();
        }
        return 480;
    }

    int getVideoBitRate(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoBitRate) != config.end())
        {
            return config[configuration::videoBitRate].as<int>();
        }
        return 400 * 1000;
    }

    std::string getFilterDescr(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::filterDescr) != config.end())
        {
            return config[configuration::filterDescr].as<std::string>();
        }
        return "";
    }
}// namespace video

namespace audio
{
    std::string getAudioName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioName) != config.end())
        {
            return config[configuration::audioName].as<std::string>();
        }
        return "chessAudio";
    }

    std::string getAudioDevice(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioDevice) != config.end())
        {
            return config[configuration::audioDevice].as<std::string>();
        }
        return "default";
    }

    int getAudioChannel(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioChannel) != config.end())
        {
            return config[configuration::audioChannel].as<int>();
        }
        return 1;
    }

    int getAudioSampleRate(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::sampleRate) != config.end())
        {
            return config[configuration::sampleRate].as<int>();
        }
        return 44100;
    }

    std::string getPlaybackDevice(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::playbackDevice) != config.end())
        {
            return config[configuration::playbackDevice].as<std::string>();
        }
        return "default";
    }

    bool enableAudioWriteToFile(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::enableWriteAudioToFile) != config.end())
        {
            return config[configuration::enableWriteAudioToFile].as<bool>();
        }
        return true;
    }

    std::string getPlaybackAudioFile(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::readTestAudioFile) != config.end())
        {
            return config[configuration::readTestAudioFile].as<std::string>();
        }
        return "";
    }

    std::string getAudioFormat(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioFormat) != config.end())
        {
            return config[configuration::audioFormat].as<std::string>();
        }
        return "PCM";
    }

	int getSampleBit(const configuration::AppConfiguration& config)
	{
		if (config.find(configuration::sampleBit) != config.end())
		{
			return config[configuration::sampleBit].as<int>();
		}
		return 8;
	}

} // namespace audio

namespace rtp
{
    int getRTPRemotePort(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::remoteRTPPort) != config.end())
        {
            return config[configuration::remoteRTPPort].as<int>();
        }
        return 9000;
    }

    std::string getRTPRemoteIpAddress(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::remoteRTPIpAddress) != config.end())
        {
            return config[configuration::remoteRTPIpAddress].as<std::string>();
        }
        return "192.168.2.102";
    }

    int getRTPLocalSendPort(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::localSendRTPPort) != config.end())
        {
            return config[configuration::localSendRTPPort].as<int>();
        }
        return 9002;
    }

    int getRTPLocalReceivePort(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::localReceiveRTPPort) != config.end())
        {
            return config[configuration::localReceiveRTPPort].as<int>();
        }
        return 9004;
    }
} // namespace rtp