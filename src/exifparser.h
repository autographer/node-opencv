/*

	Jerry: EXIF chunk extractor

	Credits: 
	
	overview: http://dev.exiv2.org/projects/exiv2/wiki/The_Metadata_in_JPEG_files
	EXIF parser: https://code.google.com/p/easyexif/source/browse/trunk/exif.cpp


*/


#ifndef EXIF_PARSER_H
#define EXIF_PARSER_H

#include <stdint.h>

namespace visorutils
{
class ExifParser
{
	//-------------------------------------------------------------------------
	enum IFFormat
	{
		IFFZero,
		IFF8,
		IFFString,
		IFF16,
		IFF32,
		IFFDouble,
	};

	//-------------------------------------------------------------------------
	// IF Entry - similar to an Excel variant ...
	struct IFEntry
	{
		// Raw fields
		uint16_t tag;
		IFFormat format;
		unsigned data;
		unsigned length;

		// Parsed fields
		uint8_t val_8;
		uint16_t val_16;
		uint32_t val_32;
		double val_rational;
		std::string val_string;
	};

	//-------------------------------------------------------------------------
	// for GPS information
	typedef struct _Coord_t
	{
		double degrees;
		double minutes;
		double seconds;
		char direction;
	} Coord_t;

	//-------------------------------------------------------------------------
	struct TagInfo
	{
		uint8_t sig;
		uint8_t marker;
		size_t offset;
		size_t size;
	};

	//-------------------------------------------------------------------------
	// JEPG/EXIF marker info
	struct MarkerInfo
	{
		uint8_t marker;
		// ASCII
		const char* name;		
		bool isVariableLength;
	};

public:

	//-------------------------------------------------------------------------
	// GPS information embedded in file
	typedef struct _Geolocation_t              
	{
		double Latitude;        // Image latitude expressed as decimal
		double Longitude;       // Image longitude expressed as decimal
		double Altitude;        // Altitude in meters, relative to sea level
		char AltitudeRef;       // 0 = above sea level, -1 = below sea level
		Coord_t LatComponents;	//
		Coord_t LonComponents;	// Latitude, Longitude expressed in deg/min/sec
		bool valid;

		_Geolocation_t() : valid(false) {}
	} GeoLocation_t;

  // 0: unspecified in EXIF data
	// 1: upper left of image
	// 3: lower right of image
	// 6: upper right of image
	// 8: lower left of image
	// 9: undefined
  typedef enum ExifOrientation{UNKNOWN_ORIENTATION=0, UPPER_LEFT=1, LOWER_RIGHT=3, UPPER_RIGHT=6, LOWER_LEFT=8};

	//-------------------------------------------------------------------------
	// Data fields filled out by parseFrom()
	struct ExifInfo
	{
		uint8_t ByteAlign;                   // 0 = Motorola byte alignment, 1 = Intel
		std::string ImageDescription;     // Image description
		std::string Make;                 // Camera manufacturer's name
		std::string Model;                // Camera model
		ExifOrientation Orientation;       // Image orientation, start of data corresponds to
		
		unsigned short BitsPerSample;     // Number of bits per component
		std::string Software;             // Software used
		std::string DateTime;             // File change date and time
		std::string DateTimeOriginal;     // Original file date and time (may not exist)
		std::string DateTimeDigitized;    // Digitization date and time (may not exist)
		std::string SubSecTimeOriginal;   // Sub-second time that original picture was taken
		std::string Copyright;            // File copyright information
		double ExposureTime;              // Exposure time in seconds
		double FNumber;                   // F/stop
		unsigned short ISOSpeedRatings;   // ISO speed
		double ShutterSpeedValue;         // Shutter speed (reciprocal of exposure time)
		double ExposureBiasValue;         // Exposure bias value in EV
		double SubjectDistance;           // Distance to focus point in meters
		double FocalLength;               // Focal length of lens in millimeters
		unsigned short FocalLengthIn35mm; // Focal length in 35mm film
		char Flash;                       // 0 = no flash, 1 = flash used
		unsigned short MeteringMode;      // Metering mode
		// 1: average
		// 2: center weighted average
		// 3: spot
		// 4: multi-spot
		// 5: multi-segment
		unsigned ImageWidth;              // Image width reported in EXIF data
		unsigned ImageHeight;             // Image height reported in EXIF data
		GeoLocation_t GeoLocation;

		ExifInfo()
		{
			reset();
		}

		void reset()
		{
			ImageWidth = 0;
			ImageHeight = 0;
			Orientation = UNKNOWN_ORIENTATION;
		}
	};

private:
	//-------------------------------------------------------------------------
	// Helper functions
	uint32_t parse32(const uint8_t* buf, bool intel)
	{
		if (intel)
		{
			return ((uint32_t)buf[3] << 24) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[1] << 8)  | buf[0];
		}
		else
		{
			return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8)  | buf[3];
		}
	}

	//-------------------------------------------------------------------------
	uint16_t parse16(const uint8_t* buf, bool intel)
	{
		if (intel)
		{
			return (((uint16_t) buf[1] << 8) | buf[0]);
		}
		else
		{
			return (((uint16_t) buf[0] << 8) | buf[1]);
		}
	}

	//-------------------------------------------------------------------------
	std::string parseEXIFString(const uint8_t* buf,
	                         const unsigned num_components,
	                         const unsigned data,
	                         const unsigned base,
	                         const unsigned len)
	{
		std::string value;
		if (num_components <= 4)
		{
			value.assign((const char*) &data, num_components );
		}
		else
		{
			if (base + data + num_components <= len)
			{
				value.assign((const char*)(buf + base + data), num_components );
			}
		}
		if(value.size() > 1 && value[value.size()-1] == '\0') {
			value = value.substr(0, value.size()-1);
		}
		if(value.size() > 0 && value[0] == '\0') {
			value = value.substr(1);
		}
		return value;
	}

	//-------------------------------------------------------------------------
	double parseEXIFRational(const unsigned char* buf, bool intel)
	{
		double numerator  = (double) parse32(buf, intel);
		double denominator = (double) parse32(buf + 4, intel);
		if (denominator < 1e-20)
		{
			return 0;
		}
		return numerator / denominator;
	}

	//-------------------------------------------------------------------------
	IFEntry parseIFEntry(const uint8_t* buf,
	                     const unsigned offs,
	                     const bool alignIntel,
	                     const unsigned base,
	                     const unsigned len)
	{
		IFEntry result;
		// Each directory entry is composed of:
		// 2 bytes: tag number (data field)
		// 2 bytes: data format
		// 4 bytes: number of components
		// 4 bytes: data value or offset to data value
		result.tag        = parse16(buf + offs, alignIntel);
		result.format     = static_cast<IFFormat>(parse16(buf + offs + 2, alignIntel));
		result.length     = parse32(buf + offs + 4, alignIntel);
		result.data       = parse32(buf + offs + 8, alignIntel);
		// Parse value in specified format
		switch (result.format)
		{
		case IFF8:
			result.val_8 = (unsigned char) * (buf + offs + 8);
			break;
		case IFFString:
			result.val_string = parseEXIFString(buf, result.length, result.data, base, len);
			break;
		case IFF16:
			result.val_16 = parse16((const unsigned char*) buf + offs + 8, alignIntel);
			break;
		case IFF32:
			result.val_32 = result.data;
			break;
		case IFFDouble:
			if (base + result.data + 8 <= len)
			{
				result.val_rational = parseEXIFRational(buf + base + result.data, alignIntel);
			}
			break;
		default:
			result.tag = 0xFF;
		}
		return result;
	}

	//-------------------------------------------------------------------------
	// is this an SOI marker
	bool IsSOI(const uint8_t* p) const
	{
		return (p[0] == M_MAGIC &&
		        p[1] == M_SOI);
	}

	//-------------------------------------------------------------------------
	// is this an EOI marker
	bool IsEOI(const uint8_t* p) const
	{
		return (p[0] == M_MAGIC &&
		        p[1] == M_EOI);
	}

	//-------------------------------------------------------------------------
	// is this an EXIF marker
	bool IsEXIF(const uint8_t* p) const
	{
		return (p[0] == M_MAGIC &&
		        p[1] == M_EXIF);
	}

	//-------------------------------------------------------------------------
	ExifInfo m_exifInfo;

	//-------------------------------------------------------------------------
	// This is horribly complicated code. 
	int parseFromEXIFSegment(const unsigned char* buf, unsigned len)
	{
		bool alignIntel = true;     // byte alignment (defined in EXIF header)
		unsigned offs   = 0;        // current offset into buffer
		if (!buf || len < 6)
		{
			return PARSE_EXIF_ERROR_NO_EXIF;
		}
		if (std::equal(buf, buf + 6, "Exif\0\0") == false)
		{
			return PARSE_EXIF_ERROR_NO_EXIF;
		}
		offs += 6;
		// Now parsing the TIFF header. The first two bytes are either "II" or
		// "MM" for Intel or Motorola byte alignment. Sanity check by parsing
		// the unsigned short that follows, making sure it equals 0x2a. The
		// last 4 bytes are an offset into the first IFD, which are added to
		// the global offset counter. For this block, we expect the following
		// minimum size:
		//  2 bytes: 'II' or 'MM'
		//  2 bytes: 0x002a
		//  4 bytes: offset to first IDF
		// -----------------------------
		//  8 bytes
		if (offs + 8 > len)
		{
			return PARSE_EXIF_ERROR_CORRUPT;
		}
		unsigned tiff_header_start = offs;
		if (buf[offs] == 'I' && buf[offs+1] == 'I')
		{
			alignIntel = true;
		}
		else
		{
			if (buf[offs] == 'M' && buf[offs+1] == 'M')
			{
				alignIntel = false;
			}
			else
			{
				return PARSE_EXIF_ERROR_UNKNOWN_BYTEALIGN;
			}
		}
		this->m_exifInfo.ByteAlign = alignIntel;
		offs += 2;
		if (0x2a != parse16(buf + offs, alignIntel))
		{
			return PARSE_EXIF_ERROR_CORRUPT;
		}
		offs += 2;
		unsigned first_ifd_offset = parse32(buf + offs, alignIntel);
		offs += first_ifd_offset - 4;
		if (offs >= len)
		{
			return PARSE_EXIF_ERROR_CORRUPT;
		}
		// Now parsing the first Image File Directory (IFD0, for the main image).
		// An IFD consists of a variable number of 12-byte directory entries. The
		// first two bytes of the IFD section contain the number of directory
		// entries in the section. The last 4 bytes of the IFD contain an offset
		// to the next IFD, which means this IFD must contain exactly 6 + 12 * num
		// bytes of data.
		if (offs + 2 > len)
		{
			return PARSE_EXIF_ERROR_CORRUPT;
		}
		int num_entries = parse16(buf + offs, alignIntel);
		if (offs + 6 + 12 * num_entries > len)
		{
			return PARSE_EXIF_ERROR_CORRUPT;
		}
		offs += 2;
		//DBMSG2("EXIF IFD: " << num_entries << " at " << offs);
		unsigned exif_sub_ifd_offset = len;
		unsigned gps_sub_ifd_offset  = len;
		while (--num_entries >= 0)
		{
			IFEntry result = parseIFEntry(buf, offs, alignIntel, tiff_header_start, len);
			//DBMSG2("Tag " << result.tag << " [" << offs << "]");
			offs += 12;
			switch (result.tag)
			{
			case 0x102:
				// Bits per sample
				if (result.format == 3)
				{
					this->m_exifInfo.BitsPerSample = result.val_16;
				}
				break;
			case 0x10E:
				// Image description
				if (result.format == 2)
				{
					this->m_exifInfo.ImageDescription = result.val_string;
				}
				break;
			case 0x10F:
				// Digicam make
				if (result.format == 2)
				{
					this->m_exifInfo.Make = result.val_string;
				}
				break;
			case 0x110:
				// Digicam model
				if (result.format == 2)
				{
					this->m_exifInfo.Model = result.val_string;
				}
				break;
			case 0x112:
				// Orientation of image
				if (result.format == 3)
				{
          this->m_exifInfo.Orientation = UNKNOWN_ORIENTATION;
          if(result.val_16 == UPPER_LEFT) {
					  this->m_exifInfo.Orientation = UPPER_LEFT;
          }
          else if(result.val_16 == UPPER_RIGHT) {
            this->m_exifInfo.Orientation = UPPER_RIGHT;
          }
          else if(result.val_16 == LOWER_LEFT) {
            this->m_exifInfo.Orientation = LOWER_LEFT;
          }
          else if(result.val_16 == LOWER_RIGHT) {
            this->m_exifInfo.Orientation = LOWER_RIGHT;
          }
				}
				break;
			case 0x131:
				// Software used for image
				if (result.format == 2)
				{
					this->m_exifInfo.Software = result.val_string;
				}
				break;
			case 0x132:
				// EXIF/TIFF date/time of image modification
				if (result.format == 2)
				{
					this->m_exifInfo.DateTime = result.val_string;
				}
				break;
			case 0x8298:
				// Copyright information
				if (result.format == 2)
				{
					this->m_exifInfo.Copyright = result.val_string;
				}
				break;
			case 0x8825:
				// GPS IFS offset
				gps_sub_ifd_offset = tiff_header_start + result.data;
				break;
			case 0x8769:
				// EXIF SubIFD offset
				exif_sub_ifd_offset = tiff_header_start + result.data;
				break;
			}
		}
		// Jump to the EXIF SubIFD if it exists and parse all the information
		// there. Note that it's possible that the EXIF SubIFD doesn't exist.
		// The EXIF SubIFD contains most of the interesting information that a
		// typical user might want.
		if (exif_sub_ifd_offset + 4 <= len)
		{
			offs = exif_sub_ifd_offset;
			int num_entries = parse16(buf + offs, alignIntel);
			if (offs + 6 + 12 * num_entries > len)
			{
				return PARSE_EXIF_ERROR_CORRUPT;
			}
			offs += 2;
//			DBMSG2("EXIF SubIFD: " << num_entries << " at " << offs);
			while (--num_entries >= 0)
			{
				IFEntry result = parseIFEntry(buf, offs, alignIntel, tiff_header_start, len);
				// DBMSG2("Tag 0x" << Chordia::toHex(result.tag) << " [" << offs << "]");
				switch (result.tag)
				{
				case 0x829a:
					// Exposure time in seconds
					if (result.format == 5)
					{
						this->m_exifInfo.ExposureTime = result.val_rational;
					}
					break;
				case 0x829d:
					// FNumber
					if (result.format == 5)
					{
						this->m_exifInfo.FNumber = result.val_rational;
					}
					break;
				case 0x8827:
					// ISO Speed Rating
					if (result.format == 3)
					{
						this->m_exifInfo.ISOSpeedRatings = result.val_16;
					}
					break;
				case 0x9003:
					// Original date and time
					if (result.format == 2)
					{
						this->m_exifInfo.DateTimeOriginal = result.val_string;
					}
					break;
				case 0x9004:
					// Digitization date and time
					if (result.format == 2)
					{
						this->m_exifInfo.DateTimeDigitized = result.val_string;
					}
					break;
				case 0x9201:
					// Shutter speed value
					if (result.format == 5)
					{
						this->m_exifInfo.ShutterSpeedValue = result.val_rational;
					}
					break;
				case 0x9204:
					// Exposure bias value
					if (result.format == 5)
					{
						this->m_exifInfo.ExposureBiasValue = result.val_rational;
					}
					break;
				case 0x9206:
					// Subject distance
					if (result.format == 5)
					{
						this->m_exifInfo.SubjectDistance = result.val_rational;
					}
					break;
				case 0x9209:
					// Flash used
					if (result.format == 3)
					{
						this->m_exifInfo.Flash = result.data ? 1 : 0;
					}
					break;
				case 0x920a:
					// Focal length
					if (result.format == 5)
					{
						this->m_exifInfo.FocalLength = result.val_rational;
					}
					break;
				case 0x9207:
					// Metering mode
					if (result.format == 3)
					{
						this->m_exifInfo.MeteringMode = result.val_16;
					}
					break;
				case 0x9291:
					// Subsecond original time
					if (result.format == 2)
					{
						this->m_exifInfo.SubSecTimeOriginal = result.val_string;
					}
					break;
				case 0xa002:
					// EXIF Image width
					if (result.format == 4)
					{
						this->m_exifInfo.ImageWidth = result.val_32;
					}
					if (result.format == 3)
					{
						this->m_exifInfo.ImageWidth = result.val_16;
					}
					break;
				case 0xa003:
					// EXIF Image height
					if (result.format == 4)
					{
						this->m_exifInfo.ImageHeight = result.val_32;
					}
					if (result.format == 3)
					{
						this->m_exifInfo.ImageHeight = result.val_16;
					}
					break;
				case 0xa405:
					// Focal length in 35mm film
					if (result.format == 3)
					{
						this->m_exifInfo.FocalLengthIn35mm = result.val_16;
					}
					break;
				}
				offs += 12;
			}
		}
		// Jump to the GPS SubIFD if it exists and parse all the information
		// there. Note that it's possible that the GPS SubIFD doesn't exist.
		if (gps_sub_ifd_offset + 4 <= len)
		{
			offs = gps_sub_ifd_offset;
			int num_entries = parse16(buf + offs, alignIntel);
			if (offs + 6 + 12 * num_entries > len)
			{
				return PARSE_EXIF_ERROR_CORRUPT;
			}
			offs += 2;

			//DBMSG2("GPS SubIFD: " << num_entries << " at " << offs);

			while (--num_entries >= 0)
			{
				unsigned short tag    = parse16(buf + offs, alignIntel);
				unsigned short format = parse16(buf + offs + 2, alignIntel);
				unsigned length       = parse32(buf + offs + 4, alignIntel);
				unsigned data         = parse32(buf + offs + 8, alignIntel);

				// DBMSG2("GPS Tag 0x" << Chordia::toHex(tag) << " [" << offs << "] : " << length);

				switch (tag)
				{
				case 1:
					// GPS north or south
					this->m_exifInfo.GeoLocation.LatComponents.direction = *(buf + offs + 8);
					if ('S' == this->m_exifInfo.GeoLocation.LatComponents.direction)
					{
						this->m_exifInfo.GeoLocation.Latitude = -this->m_exifInfo.GeoLocation.Latitude;
					}
					break;
				case 2:
					// GPS latitude
					if (format == 5 && length == 3)
					{
						this->m_exifInfo.GeoLocation.LatComponents.degrees =
						    parseEXIFRational(buf + data + tiff_header_start, alignIntel);
						this->m_exifInfo.GeoLocation.LatComponents.minutes =
						    parseEXIFRational(buf + data + tiff_header_start + 8, alignIntel);
						this->m_exifInfo.GeoLocation.LatComponents.seconds =
						    parseEXIFRational(buf + data + tiff_header_start + 16, alignIntel);
						this->m_exifInfo.GeoLocation.Latitude =
						    this->m_exifInfo.GeoLocation.LatComponents.degrees +
						    this->m_exifInfo.GeoLocation.LatComponents.minutes / 60 +
						    this->m_exifInfo.GeoLocation.LatComponents.seconds / 3600;
						if ('S' == this->m_exifInfo.GeoLocation.LatComponents.direction)
						{
							this->m_exifInfo.GeoLocation.Latitude = -this->m_exifInfo.GeoLocation.Latitude;
						}
						this->m_exifInfo.GeoLocation.valid = true;
					}
					break;
				case 3:
					// GPS east or west
					this->m_exifInfo.GeoLocation.LonComponents.direction = *(buf + offs + 8);
					if ('W' == this->m_exifInfo.GeoLocation.LonComponents.direction)
					{
						this->m_exifInfo.GeoLocation.Longitude = -this->m_exifInfo.GeoLocation.Longitude;
					}
					break;
				case 4:
					// GPS longitude
					if (format == 5 && length == 3)
					{
						this->m_exifInfo.GeoLocation.LonComponents.degrees =
						    parseEXIFRational(buf + data + tiff_header_start, alignIntel);
						this->m_exifInfo.GeoLocation.LonComponents.minutes =
						    parseEXIFRational(buf + data + tiff_header_start + 8, alignIntel);
						this->m_exifInfo.GeoLocation.LonComponents.seconds =
						    parseEXIFRational(buf + data + tiff_header_start + 16, alignIntel);
						this->m_exifInfo.GeoLocation.Longitude =
						    this->m_exifInfo.GeoLocation.LonComponents.degrees +
						    this->m_exifInfo.GeoLocation.LonComponents.minutes / 60 +
						    this->m_exifInfo.GeoLocation.LonComponents.seconds / 3600;
						if ('W' == this->m_exifInfo.GeoLocation.LonComponents.direction)
						{
							this->m_exifInfo.GeoLocation.Longitude = -this->m_exifInfo.GeoLocation.Longitude;
						}
					}
					break;
				case 5:
					// GPS altitude reference (below or above sea level)
					this->m_exifInfo.GeoLocation.AltitudeRef = *(buf + offs + 8);
					if (1 == this->m_exifInfo.GeoLocation.AltitudeRef)
					{
						this->m_exifInfo.GeoLocation.Altitude = -this->m_exifInfo.GeoLocation.Altitude;
					}
					break;
				case 6:
					// GPS altitude reference
					if (format == 5)
					{
						this->m_exifInfo.GeoLocation.Altitude =
						    parseEXIFRational(buf + data + tiff_header_start, alignIntel);
						if (1 == this->m_exifInfo.GeoLocation.AltitudeRef)
						{
							this->m_exifInfo.GeoLocation.Altitude = -this->m_exifInfo.GeoLocation.Altitude;
						}
					}
					break;
				}
				offs += 12;
			}
		}
		// DBMSG2("EXIF parse completed: " << offs << " [0x" << toHex(buf[offs]) << "]");
		return PARSE_EXIF_SUCCESS;
	}

	//-----------------------------------------------------------------------------
	// lookup set of known JPEG/EXIF tags
	std::map<uint8_t,MarkerInfo> m_markers;
	typedef std::map<uint8_t,MarkerInfo>::iterator marker_iterator;

	//-----------------------------------------------------------------------------
	// match a M_MAGIC and M_XXX pair of bytes
	bool IsValidTag(const uint8_t* ps)
	{
		return (ps[0] == M_MAGIC && 
				m_markers.find(ps[1]) != m_markers.end());
	}

	//-----------------------------------------------------------------------------
	// vector of offset and sizes
	std::vector<TagInfo> m_tags;

	//-----------------------------------------------------------------------------
	// the EXIF data buffer
	std::vector<uint8_t> m_exifData;
	//
	size_t m_offset;

public:

	//-----------------------------------------------------------------------------
	// EXIF/JPEG tags of interest
	enum
	{
		M_SOF0 = 0xC0,          // Start Of Frame N
		M_SOF1 = 0xC1,          // N indicates which compression process
		M_SOF2 = 0xC2,          // Only SOF0-SOF2 are now in common use
		M_DHT  = 0xC4,          // Define Huffmann Table
		M_SOF3 = 0xC3,
		M_SOF5 = 0xC5,          // NB: codes C4 and CC are NOT SOF markers
		M_SOF6 = 0xC6,
		M_SOF7 = 0xC7,
		M_SOF9 = 0xC9,
		M_SOF10 = 0xCA,
		M_SOF11 = 0xCB,
		M_SOF13 = 0xCD,
		M_SOF14 = 0xCE,
		M_SOF15 = 0xCF,
		M_SOI = 0xD8,          // Start Of Image (beginning of datastream)
		M_EOI = 0xD9,          // End Of Image (end of datastream)
		M_DQT  = 0xDB,          // Define Quantization Table
		M_DRI  = 0xDD,
		M_SOS = 0xDA,          // Start Of Scan (begins compressed data)
		M_JFIF = 0xE0,          // Jfif marker
		M_IPTC = 0xED,          // IPTC marker
		M_EXIF = 0xE1,         // Exif marker.  Also used for XMP data!
		M_COM  = 0xFE,          // COMment
		M_MAGIC = 0xFF,
		// Parse was successful
		PARSE_EXIF_SUCCESS = 0xAAAA,
		// No JPEG markers found in buffer, possibly invalid JPEG file
		PARSE_EXIF_ERROR_NO_JPEG = 0xFFFF,
		// No EXIF header found in JPEG file.
		PARSE_EXIF_ERROR_NO_EXIF,
		// Byte alignment specified in EXIF file was unknown (not Motorola or Intel).
		PARSE_EXIF_ERROR_UNKNOWN_BYTEALIGN,
		// EXIF header was found, but data was corrupted.
		PARSE_EXIF_ERROR_CORRUPT,
	};

	//-----------------------------------------------------------------------------
	ExifParser() : m_offset(0)
	{
		MarkerInfo markers[] = 
		{
			{ M_SOF0, "SOF0", true },
			{ M_SOF1, "SOF1", true },
			{ M_SOF2, "SOF2", true },
			{ M_SOF3, "SOF3", true },
			{ M_SOF5, "SOF5", true },
			{ M_SOF6, "SOF6", true },
			{ M_SOF7, "SOF7", true },
			{ M_SOF9, "SOF9", true },
			{ M_SOF10, "SOF10", true },
			{ M_SOF11, "SOF11", true },
			{ M_SOF13, "SOF13", true },
			{ M_SOF14, "SOF14", true },
			{ M_SOF15, "SOF15", true },
			{ M_SOI, "SOI", false },
			{ M_EOI, "EOI", false },
			{ M_SOS, "SOS", true },
			{ M_JFIF, "JFIF", true },
			{ M_EXIF, "EXIF", true },
			{ M_COM,  "COM", true },
			{ M_DQT,  "DQT", true },
			{ M_DHT,  "DHT", true },
			{ M_DRI,  "DRI", true },
			{ M_IPTC, "IPTC", true },
			{ M_MAGIC },	// terminator. Do not remove
		};

		// set up a map of known tags for valid lookup
		for (int s = 0; markers[s].marker!= M_MAGIC; s++)
		{
			m_markers[markers[s].marker] = markers[s];
		}
	}

	//-----------------------------------------------------------------------------
	// EXIF data accessors
	size_t size() const
	{
		return m_exifData.size();
	}
	// 
	const uint8_t* begin() 
	{
		const uint8_t* p = &m_exifData[0];
		return p;
	}

	const uint8_t* end()
	{
		return begin() + size();
	}
	
	//-----------------------------------------------------------------------------
	// onlu valid if size() > 0
	size_t offset() const 
	{
		return m_offset;
	}
	
	//-----------------------------------------------------------------------------
	bool ExtractTags(const std::vector<uint8_t>& buffer)
	{
		m_offset = 0;
		m_exifData.clear();
		//
		m_tags.clear();
		// Sanity check: all JPEG files start with 0xFFD8 and end with 0xFFD9
		// This check also ensures that the user has supplied a correct value for len.
		size_t size = buffer.size();
		if (size < 4)
		{
			return false;
		}
		if (IsSOI(&buffer[0]) == false)
		{
			return false;
		}
		if (IsEOI(&buffer[size-2]) == false)
		{
			return false;
		}
		// iterate over buffer. 
		size_t offs = 0;
		//
		const uint8_t* ps = &buffer[0];
		// the last pair we can check has already been validated at this stage
		// hence the -1 so we do not overrun end of buffer
		const uint8_t* pe = (ps + size) - 1;
		// find all the markers
		const uint8_t* pc = ps;
		//
		uint16_t section_length = 0;
		//
		//
		for (pc = ps; pc < pe; pc++, offs++)
		{
			if (IsValidTag(pc))
			{
				bool isExif = IsEXIF(pc);
				marker_iterator mit = m_markers.find(pc[1]);
				// SOI and EOI do not carry segment length
				if (mit == m_markers.end())
				{
					// this is an error ...
					break;
				}
				//
				if (mit->second.isVariableLength)
				{
					// account for 2 bytes of marker data
					section_length = parse16(pc + 2, false) + 2;
				}
				else
				{
					// jeez guys. WTF not keep it consistent ...!
					section_length = 2;
				}
				// grab the EXIF data
				if (isExif)
				{
					m_exifData.assign(pc,pc+section_length);
					m_offset = offs;
				}
				//
				// DBMSG2("[" << offs << "] " << section_length << " 0x" << toHex(pc[0]) << toHex(pc[1]) << " " << mit->second.name);

				TagInfo ti;
				ti.sig = pc[0];
				ti.marker = pc[1];
				ti.offset = offs;
				ti.size = section_length;
				m_tags.push_back(ti);
			}
		}
		return (m_tags.size() > 0);
	}

	const ExifInfo GetExifInfo() const {return m_exifInfo;}
	
  bool ParseExifData(const std::string& full_path)
  {
    bool success = false;
    FILE *fp = fopen(full_path.c_str(), "rb");
    if(fp)
    {
      if(fseek(fp, 0, SEEK_END) == 0)
      {
        size_t file_size = ftell(fp);
        if(file_size < 20000000) // sanity check size
        {
          std::vector<uint8_t> buf(file_size, 0);
          fseek(fp, 0, SEEK_SET);
          size_t nbytes = fread(&buf[0], 1, file_size, fp);
          if(ParseExifData(buf))
          {
            success = true;
          }
        }
      }
      fclose(fp);
    }
    return success;
  }

  bool ReadFile(const std::string& src, std::vector<uint8_t>& buf) {
    bool success = false;
    buf.clear();

    FILE *fp = fopen(src.c_str(), "rb");
    if (fp)
    {
      if (fseek(fp, 0, SEEK_END) == 0)
      {
        size_t file_size = ftell(fp);
        if (file_size < 20000000) // sanity check size
        {
          buf.resize(file_size, 0);
          fseek(fp, 0, SEEK_SET);
          size_t nbytes = fread(&buf[0], 1, file_size, fp);
          success = (nbytes == file_size);
          fclose(fp);
        }
      }
    }
    return success;
  }

  bool CopyExifData(const std::string& src, const std::string& dest)
  {
    std::vector<uint8_t> buf, destbuf;
    bool success = false;
    
    if(ReadFile(src, buf))
    {
      size_t start, size;
      if (ParseExifSize(buf, start, size))
      {
        if(ReadFile(dest, destbuf))
        {
          std::vector<uint8_t> out;

          out.push_back(0xFF);
          out.push_back(0xD8);

          for (size_t n = 0; n < size; n++) {
            out.push_back(buf[n+start]);
          }
          for (size_t n = 2; n < destbuf.size(); n++) {
            out.push_back(destbuf[n]);
          }

          FILE *fp = fopen(dest.c_str(), "wb");
          if (fp) {
            fwrite(&out[0], out.size(), 1, fp);
            fclose(fp);
          }
          success = true;
        }
      }
    }
    return success;
  }

  bool ParseExifSize(const std::vector<uint8_t>& buffer, 
                     size_t& section_start, 
                     size_t& section_length)
  {
    // Sanity check: all JPEG files start with 0xFFD8 and end with 0xFFD9
    // This check also ensures that the user has supplied a correct value for len.
    size_t size = buffer.size();
    if (size < 4)
    {
      return false;
    }
    if (IsSOI(&buffer[0]) == false)
    {
      return false;
    }
    /*if (IsEOI(&buffer[size-2]) == false)
    {
    return false;
    }*/
    // Scan for EXIF header (bytes 0xFF 0xE1) and do a sanity check by
    // looking for bytes "Exif\0\0". The marker length data is in Motorola
    // byte order, which results in the 'false' parameter to parse16().
    // The marker has to contain at least the TIFF header, otherwise the
    // EXIF data is corrupt. So the minimum length specified here has to be:
    //   2 bytes: section size
    //   6 bytes: "Exif\0\0" string
    //   2 bytes: TIFF header (either "II" or "MM" string)
    //   2 bytes: TIFF magic (short 0x2a00 in Motorola byte order)
    //   4 bytes: Offset to first IFD
    // =========
    //  16 bytes
    size_t offs = 0;        // current offset into buffer
    for (offs = 0; offs < size - 1; offs++)
    {
      if (IsEXIF(&buffer[offs]))
      {
        break;
      }
    }
    if (offs + 4 > size)
    {
      return false;
    }
    
    section_start = offs;

    // step over marker
    offs += 2;
    // extract tag length -always BE (Motorola format)
    section_length = parse16(&buffer[0] + offs, false);

    if (offs + section_length > size || section_length < 16)
    {
      return false;
    }

    section_length += 2; // 0xFFE1 (size excludes marker but includes size bytes)

    return true;
  }

	//-----------------------------------------------------------------------------
	// Locates the EXIF segment and parses it using parseFromEXIFSegment
	bool ParseExifData(const std::vector<uint8_t>& buffer)
	{
		m_exifInfo.reset();

    size_t size = buffer.size();

    size_t section_start = 0, section_length = 0;

    if (!ParseExifSize(buffer, section_start, section_length)) {
      return false;
    }

		// extract EXIF elements
    uint32_t off = section_start + 4; // 4 is the offset to ExifHeader start
    bool ret = (parseFromEXIFSegment(&buffer[off], size - off) == PARSE_EXIF_SUCCESS);
		// sanity check

		uint8_t magic = buffer[section_start + section_length];
		//
		ret &= (magic == M_MAGIC);
		//
		// DBMSG2("Parsed EXIF " << ret << " next marker is 0x" << toHex(magic) << toHex(buffer[section_start+section_length+1]));
		//
		return ret;
		// return true;
	}
};	// class

}	// namespace

#endif
