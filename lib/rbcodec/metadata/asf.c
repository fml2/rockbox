/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * $Id$
 *
 * Copyright (C) 2007 Dave Chapman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include "platform.h"

#include "metadata.h"
#include "replaygain.h"
#include "debug.h"
#include "rbunicode.h"
#include "metadata_common.h"
#include "metadata_parsers.h"
#include <codecs/libasf/asf.h>
#include "rbunicode.h"
/* TODO: Just read the GUIDs into a 16-byte array, and use memcmp to compare */
struct guid_s {
    uint32_t v1;
    uint16_t v2;
    uint16_t v3;
    uint8_t  v4[8];
};
typedef struct guid_s guid_t;

struct asf_object_s {
    guid_t       guid;
    uint64_t     size;
    uint64_t     datalen;
};
typedef struct asf_object_s asf_object_t;

#if 0
static const guid_t asf_guid_null =
{0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

/* top level object guids */

static const guid_t asf_guid_header =
{0x75B22630, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};

static const guid_t asf_guid_index =
{0x33000890, 0xE5B1, 0x11CF, {0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB}};
#endif

static const guid_t asf_guid_data =
{0x75B22636, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};

/* header level object guids */

static const guid_t asf_guid_file_properties =
{0x8cabdca1, 0xa947, 0x11cf, {0x8E, 0xe4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};

static const guid_t asf_guid_stream_properties =
{0xB7DC0791, 0xA9B7, 0x11CF, {0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};

static const guid_t asf_guid_content_description =
{0x75B22633, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};

static const guid_t asf_guid_extended_content_description =
{0xD2D0A440, 0xE307, 0x11D2, {0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50}};

static const guid_t asf_guid_content_encryption =
{0x2211b3fb, 0xbd23, 0x11d2, {0xb4, 0xb7, 0x00, 0xa0, 0xc9, 0x55, 0xfc, 0x6e}};

static const guid_t asf_guid_extended_content_encryption =
{0x298ae614, 0x2622, 0x4c17, {0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c}};

/* stream type guids */

static const guid_t asf_guid_stream_type_audio =
{0xF8699E40, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};

static int asf_guid_match(const guid_t *guid1, const guid_t *guid2)
{
    if((guid1->v1 != guid2->v1) ||
       (guid1->v2 != guid2->v2) ||
       (guid1->v3 != guid2->v3) ||
       (memcmp(guid1->v4, guid2->v4, 8))) {
        return 0;
    }

    return 1;
}

/* Read the 16 byte GUID from a file */
static void asf_readGUID(int fd, guid_t* guid)
{
    int bytes;
    bytes = read_uint32le(fd, &guid->v1);
    bytes += read_uint16le(fd, &guid->v2);
    bytes += read_uint16le(fd, &guid->v3);
    bytes += read(fd, guid->v4, 8);
    if (bytes != sizeof(guid_t))
        memset(guid, 0, sizeof(guid_t));
}

static void asf_read_object_header(asf_object_t *obj, int fd)
{
    asf_readGUID(fd, &obj->guid);

    if (read_uint64le(fd, &obj->size) != sizeof (uint64_t))
        obj->size = 0;
    obj->datalen = 0;
}

/* Parse an integer from the extended content object - we always
   convert to an int, regardless of native format.
*/
static int asf_intdecode(int fd, int type, int length)
{
    int bytes = 0;
    int ret;
    union {
        uint16_t tmp16;
        uint32_t tmp32;
        uint64_t tmp64;
    } uu = {0};

    if (type == 3) {
        bytes = read_uint32le(fd, &uu.tmp32);
        ret = (int)uu.tmp32;
    } else if (type == 4) {
        bytes = read_uint64le(fd, &uu.tmp64);
        ret = (int)uu.tmp64;
    } else if (type == 5) {
        bytes = read_uint16le(fd, &uu.tmp16);
        ret = (int)uu.tmp16;
    }

    if (bytes > 0)
    {
        lseek(fd,length - bytes, SEEK_CUR);
        return ret;
    }
    return 0;
}

static int is_valid_utf16(const unsigned char *data, size_t length)
{
    if (length < 2) return 0; // Not enough data for even one UTF-16 character

    // Get the last two bytes as a UTF-16 character (little-endian)
    uint16_t last = data[length - 2] | (data[length - 1] << 8);

    // Check if the last character is a high surrogate
    if (last >= 0xD800 && last <= 0xDBFF) {
        return 0; // Invalid if it's the last character
    }

    // Check if the last character is a low surrogate
    if (last >= 0xDC00 && last <= 0xDFFF) {
        if (length < 4) return 0; // Invalid if there's no preceding character
        uint16_t second_last = data[length - 4] | (data[length - 3] << 8);

        // Invalid if not preceded by a high surrogate
        return second_last >= 0xD800 && second_last <= 0xDBFF; 
    }

    // If it's not a surrogate, it's valid
    return 1;
}

/* Decode a LE utf16 string from a disk buffer into a fixed-sized
   utf8 buffer.
*/
static void asf_utf16LEdecode(int fd,
                              uint16_t utf16bytes,
                              unsigned char **utf8,
                              int* utf8bytes
                             )
{
    const int reserve_bytes = 6;
    int n;
    unsigned char utf16buf[258];
    unsigned char* newutf8 = *utf8;
    const int utf8bytes_initial = *utf8bytes;

    while ((n = read(fd, utf16buf, MIN(sizeof(utf16buf) - 2, utf16bytes))) >= 2)
    {
        // If the UTF-16 string ends with an incomplete surrogate pair, try to complete it.
        if (!is_valid_utf16(utf16buf, n))
        {
            n += read(fd, utf16buf + n, 2);
        }
        newutf8 = utf16decode(utf16buf, newutf8, n>>1, *utf8bytes - reserve_bytes, true);
        *utf8bytes = utf8bytes_initial - (newutf8 - *utf8);
        utf16bytes -= n;

        if (*utf8bytes <= reserve_bytes)
            break;
    }

    *newutf8 = 0;
    --*utf8bytes;
    *utf8 = newutf8;

    if (utf16bytes > 0) {
        /* Skip any remaining bytes */
        lseek(fd, utf16bytes, SEEK_CUR);
    }
}

static int asf_parse_header(int fd, struct mp3entry* id3,
                                    asf_waveformatex_t* wfx)
{
    asf_object_t current;
    asf_object_t header;
    uint64_t datalen;
    int i;
    int fileprop = 0;
    uint64_t play_duration;
    uint16_t flags;
    uint32_t subobjects;
    uint8_t utf8buf[512];
    int id3buf_remaining = sizeof(id3->id3v2buf) + sizeof(id3->id3v1buf);
    unsigned char* id3buf = (unsigned char*)id3->id3v2buf;

    asf_read_object_header((asf_object_t *) &header, fd);

    //DEBUGF("header.size=%d\n",(int)header.size);
    if (header.size < 30) {
        /* invalid size for header object */
        return ASF_ERROR_OBJECT_SIZE;
    }

    read_uint32le(fd, &subobjects);

    /* Two reserved bytes - do we need to read them? */
    lseek(fd, 2, SEEK_CUR);

    //DEBUGF("Read header - size=%d, subobjects=%d\n",(int)header.size, (int)subobjects);

    if (subobjects > 0) {
        header.datalen = header.size - 30;

        /* TODO: Check that we have datalen bytes left in the file */
        datalen = header.datalen;

        for (i=0; i<(int)subobjects; i++) {
            //DEBUGF("Parsing header object %d - datalen=%d\n",i,(int)datalen);
            if (datalen < 24) {
                //DEBUGF("not enough data for reading object\n");
                break;
            }

            asf_read_object_header(&current, fd);

            if (current.size > datalen || current.size < 24) {
                //DEBUGF("invalid object size - current.size=%d, datalen=%d\n",(int)current.size,(int)datalen);
                break;
            }

            if (asf_guid_match(&current.guid, &asf_guid_file_properties)) {
                    if (current.size < 104)
                        return ASF_ERROR_OBJECT_SIZE;

                    if (fileprop) {
                        /* multiple file properties objects not allowed */
                        return ASF_ERROR_INVALID_OBJECT;
                    }

                    fileprop = 1;

                    /* Get the number of logical packets - uint64_t at offset 32
                     * (little endian byte order) */
                    lseek(fd, 32, SEEK_CUR);
                    read_uint64le(fd, &wfx->numpackets);
                    /*DEBUGF("read packets:  %llx %lld\n", wfx->numpackets, wfx->numpackets);*/

                    /* Now get the play duration - uint64_t at offset 40 */
                    read_uint64le(fd, &play_duration);
                    id3->length = play_duration / 10000;
                    /*DEBUGF("****** length = %lums\n", id3->length);*/

                    /* Read the packet size - uint32_t at offset 68 */
                    lseek(fd, 20, SEEK_CUR);
                    read_uint32le(fd, &wfx->packet_size);

                    /* Skip bytes remaining in object */
                    lseek(fd, current.size - 24 - 72, SEEK_CUR);
            } else if (asf_guid_match(&current.guid, &asf_guid_stream_properties)) {
                    guid_t guid;
                    uint32_t propdatalen;

                    if (current.size < 78)
                        return ASF_ERROR_OBJECT_SIZE;

#if 0
                    asf_byteio_getGUID(&guid, current->data);
                    datalen = asf_byteio_getDWLE(current->data + 40);
                    flags = asf_byteio_getWLE(current->data + 48);
#endif

                    asf_readGUID(fd, &guid);

                    lseek(fd, 24, SEEK_CUR);
                    read_uint32le(fd, &propdatalen);
                    lseek(fd, 4, SEEK_CUR);
                    read_uint16le(fd, &flags);

                    if (!asf_guid_match(&guid, &asf_guid_stream_type_audio)) {
                        //DEBUGF("Found stream properties for non audio stream, skipping\n");
                        lseek(fd,current.size - 24 - 50,SEEK_CUR);
                    } else if (wfx->audiostream == -1) {
                        lseek(fd, 4, SEEK_CUR);
                        //DEBUGF("Found stream properties for audio stream %d\n",flags&0x7f);

                        if (propdatalen < 18) {
                            return ASF_ERROR_INVALID_LENGTH;
                        }

#if 0
                        if (asf_byteio_getWLE(data + 16) > datalen - 16) {
                            return ASF_ERROR_INVALID_LENGTH;
                        }
#endif
                        read_uint16le(fd, &wfx->codec_id);
                        read_uint16le(fd, &wfx->channels);
                        read_uint32le(fd, &wfx->rate);
                        read_uint32le(fd, &wfx->bitrate);
                        wfx->bitrate *= 8;
                        read_uint16le(fd, &wfx->blockalign);
                        read_uint16le(fd, &wfx->bitspersample);
                        read_uint16le(fd, &wfx->datalen);

                        /*sanity check the included bitrate by comparing to file size and length*/
                        unsigned int estimated_bitrate = id3->length ? (wfx->packet_size*wfx->numpackets)/id3->length*8000 : 0;

                        /*in theory we could just use the estimated bitrate always,
                          but its safer to underestimate*/
                        if( wfx->bitrate > estimated_bitrate)
                        {
                            /* Round bitrate to the nearest kbit */
                            id3->bitrate = (estimated_bitrate + 500) / 1000;
                        }
                        else
                        {
                            /* Round bitrate to the nearest kbit */
                            id3->bitrate = (wfx->bitrate + 500) / 1000;
                        }
                        /*DEBUGF("bitrate:  %d estimated:  %d\n", wfx->bitrate, estimated_bitrate);*/
                        id3->frequency = wfx->rate;

                        if (wfx->codec_id == ASF_CODEC_ID_WMAV1) {
                            read(fd, wfx->data, 4);
                            lseek(fd,current.size - 24 - 72 - 4,SEEK_CUR);
                            wfx->audiostream = flags&0x7f;
                        } else if (wfx->codec_id == ASF_CODEC_ID_WMAV2) {
                            read(fd, wfx->data, 6);
                            lseek(fd,current.size - 24 - 72 - 6,SEEK_CUR);
                            wfx->audiostream = flags&0x7f;
                        } else if (wfx->codec_id == ASF_CODEC_ID_WMAPRO) {
                            /* wma pro decoder needs the extra-data */
                            read(fd, wfx->data, wfx->datalen);
                            lseek(fd,current.size - 24 - 72 - wfx->datalen,SEEK_CUR);
                            wfx->audiostream = flags&0x7f;
                            /* Correct codectype to redirect playback to the proper .codec */
                            id3->codectype = AFMT_WMAPRO;
                        } else if (wfx->codec_id == ASF_CODEC_ID_WMAVOICE) {
                            read(fd, wfx->data, wfx->datalen);
                            lseek(fd,current.size - 24 - 72 - wfx->datalen,SEEK_CUR);
                            wfx->audiostream = flags&0x7f;
                            id3->codectype = AFMT_WMAVOICE;
                        } else if (wfx->codec_id == ASF_CODEC_ID_MP3) {
                            lseek(fd,current.size - 24 - 72,SEEK_CUR);
                            wfx->audiostream = flags&0x7f;
                            id3->codectype = AFMT_MPA_L3;
                            id3->is_asf_stream = true;
                        } else {
                            DEBUGF("Unsupported WMA codec (Lossless, Voice, etc)\n");
                            lseek(fd,current.size - 24 - 72,SEEK_CUR);
                        }

                    }
            } else if (asf_guid_match(&current.guid, &asf_guid_content_description)) {
                    /* Object contains five 16-bit string lengths, followed by the five strings:
                       title, artist, copyright, description, rating
                     */
                    uint16_t strlength[5];
                    int i;

                    //DEBUGF("Found GUID_CONTENT_DESCRIPTION - size=%d\n",(int)(current.size - 24));

                    /* Read the 5 string lengths - number of bytes included trailing zero */
                    for (i=0; i<5; i++) {
                        read_uint16le(fd, &strlength[i]);
                        //DEBUGF("strlength = %u\n",strlength[i]);
                    }

                    if (strlength[0] > 0) {  /* 0 - Title */
                        id3->title = id3buf;
                        asf_utf16LEdecode(fd, strlength[0], &id3buf, &id3buf_remaining);
                    }

                    if (strlength[1] > 0) {  /* 1 - Artist */
                        id3->artist = id3buf;
                        asf_utf16LEdecode(fd, strlength[1], &id3buf, &id3buf_remaining);
                    }

                    lseek(fd, strlength[2], SEEK_CUR); /* 2 - copyright */

                    if (strlength[3] > 0) {  /* 3 - description */
                        id3->comment = id3buf;
                        asf_utf16LEdecode(fd, strlength[3], &id3buf, &id3buf_remaining);
                    }

                    lseek(fd, strlength[4], SEEK_CUR); /* 4 - rating */
            } else if (asf_guid_match(&current.guid, &asf_guid_extended_content_description)) {
                    uint16_t count;
                    int i;
                    int bytesleft = current.size - 24;
                    //DEBUGF("Found GUID_EXTENDED_CONTENT_DESCRIPTION\n");

                    read_uint16le(fd, &count);
                    bytesleft -= 2;
                    //DEBUGF("extended metadata count = %u\n",count);
                    enum
                    {
                        eWM_TrackNumber, eWM_Genre, eWM_AlbumTitle,
                        eWM_AlbumArtist, eWM_Composer, eWM_Year,
                        eWM_MusicBrainz_Track_Id, eWM_Picture,
                        eWM_COUNT_TAG_COUNT
                    };

                    static const char *tagops[eWM_COUNT_TAG_COUNT + 1] =
                    { [eWM_TrackNumber] = "WM/TrackNumber",
                      [eWM_Genre] = "WM/Genre",
                      [eWM_AlbumTitle] = "WM/AlbumTitle",
                      [eWM_AlbumArtist] = "WM/AlbumArtist",
                      [eWM_Composer] = "WM/Composer",
                      [eWM_Year] = "WM/Year",
                      [eWM_MusicBrainz_Track_Id]"MusicBrainz/Track Id",
                      [eWM_Picture]"WM/Picture",
                      [eWM_COUNT_TAG_COUNT] = NULL
                    };

                    for (i=0; i < count; i++) {
                        uint16_t length, type;
                        unsigned char* utf8 = utf8buf;
                        int utf8length = 512;

                        read_uint16le(fd, &length);
                        asf_utf16LEdecode(fd, length, &utf8, &utf8length);
                        bytesleft -= 2 + length;

                        read_uint16le(fd, &type);
                        read_uint16le(fd, &length);

                        int itag = string_option(utf8buf, tagops, false);

                        if (itag == eWM_TrackNumber) {
                            if (type == 0) {
                                id3->track_string = id3buf;
                                asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                                id3->tracknum = atoi(id3->track_string);
                            } else if ((type >=2) && (type <= 5)) {
                                id3->tracknum = asf_intdecode(fd, type, length);
                            } else {
                                lseek(fd, length, SEEK_CUR);
                            }
                        } else if ((itag == eWM_Genre) && (type == 0)) {
                            id3->genre_string = id3buf;
                            asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                        } else if ((itag == eWM_AlbumTitle) && (type == 0)) {
                            id3->album = id3buf;
                            asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                        } else if ((itag == eWM_AlbumArtist) && (type == 0)) {
                            id3->albumartist = id3buf;
                            asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                        } else if ((itag == eWM_Composer) && (type == 0)) {
                            id3->composer = id3buf;
                            asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                        } else if (itag == eWM_Year) {
                            if (type == 0) {
                                id3->year_string = id3buf;
                                asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                                id3->year = atoi(id3->year_string);
                            } else if ((type >=2) && (type <= 5)) {
                                id3->year = asf_intdecode(fd, type, length);
                            } else {
                                lseek(fd, length, SEEK_CUR);
                            }
                        } else if (itag == eWM_MusicBrainz_Track_Id) {
                            id3->mb_track_id = id3buf;
                            asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
#ifdef HAVE_ALBUMART
                        } else if (itag == eWM_Picture) {
                            /* Expected is either "01 00 xx xx 03 yy yy yy yy" or
                             * "03 yy yy yy yy". xx is the size of the WM/Picture
                             * container in bytes. yy equals the raw data length of
                             * the embedded image.
                             * 
                             *  Also save position after this tag in file in case any parsing errors */
                            uint32_t after_pic_pos = lseek(fd, -4, SEEK_CUR) + 4 + length;

                            if (read(fd, &type, 1) != 1)
                                type = 0;

                            if (type == 1) {
                                lseek(fd, 3, SEEK_CUR);
                                read(fd, &type, 1);
                            }
                            if (type == 3) {
                                uint32_t pic_size = 0;
                                /* Read the raw data length of the embedded image. */
                                read_uint32le(fd, &pic_size);

                                /* Reset utf8 buffer */
                                utf8 = utf8buf;
                                utf8length = 512;

                                /* Gather the album art format, this string has a
                                 * double zero-termination. */
                                asf_utf16LEdecode(fd, 32, &utf8, &utf8length);

                                static const char *aa_options[] = {"image/jpeg",
                                                   "image/jpg","image/png", NULL};
                                int aa_op = string_option(utf8buf, aa_options, false);

                                if (aa_op == 0) /*image/jpeg*/
                                {
                                    id3->albumart.type = AA_TYPE_JPG;
                                }
                                else if (aa_op == 1) /*image/jpg*/
                                {
                                    /* image/jpg is technically invalid,
                                     * but it does occur in the wild */
                                    id3->albumart.type = AA_TYPE_JPG;
                                }
                                else if (aa_op == 2) /*image/png*/
                                {
                                    id3->albumart.type = AA_TYPE_PNG;
                                } else {
                                    id3->albumart.type = AA_TYPE_UNKNOWN;
                                }

                                /* Set the album art size and position. */
                                if (id3->albumart.type != AA_TYPE_UNKNOWN) {
                                    id3->albumart.pos  = after_pic_pos - pic_size;
                                    id3->albumart.size = pic_size;
                                    id3->has_embedded_albumart = true;
                                }
                            }

                            lseek(fd, after_pic_pos, SEEK_SET);
#endif
                        } else if (!strncmp("replaygain_", utf8buf, 11)) {
                            char *value = id3buf;
                            asf_utf16LEdecode(fd, length, &id3buf, &id3buf_remaining);
                            parse_replaygain(utf8buf, value, id3);
                        } else {
                            lseek(fd, length, SEEK_CUR);
                        }
                        bytesleft -= 4 + length;
                    }

                    lseek(fd, bytesleft, SEEK_CUR);
            } else if (asf_guid_match(&current.guid, &asf_guid_content_encryption)
                || asf_guid_match(&current.guid, &asf_guid_extended_content_encryption)) {
                //DEBUGF("File is encrypted\n");
                return ASF_ERROR_ENCRYPTED;
            } else {
                //DEBUGF("Skipping %d bytes of object\n",(int)(current.size - 24));
                lseek(fd,current.size - 24,SEEK_CUR);
            }

            //DEBUGF("Parsed object - size = %d\n",(int)current.size);
            datalen -= current.size;
        }

        if (i != (int)subobjects || datalen != 0) {
            //DEBUGF("header data doesn't match given subobject count\n");
            return ASF_ERROR_INVALID_VALUE;
        }

        //DEBUGF("%d subobjects read successfully\n", i);
    }

#if 0
    tmp = asf_parse_header_validate(file, &header);
    if (tmp < 0) {
        /* header read ok but doesn't validate correctly */
        return tmp;
    }
#endif

    //DEBUGF("header validated correctly\n");

    return 0;
}

bool get_asf_metadata(int fd, struct mp3entry* id3)
{
    int res;
    asf_object_t obj;
    asf_waveformatex_t wfx;

    wfx.audiostream = -1;

    res = asf_parse_header(fd, id3, &wfx);

    if (res < 0) {
        DEBUGF("ASF: parsing error - %d\n",res);
        return false;
    }

    if (wfx.audiostream == -1) {
        DEBUGF("ASF: No WMA streams found\n");
        return false;
    }

    asf_read_object_header(&obj, fd);

    if (!asf_guid_match(&obj.guid, &asf_guid_data)) {
        DEBUGF("ASF: No data object found\n");
        return false;
    }

    /* Store the current file position - no need to parse the header
       again in the codec.  The +26 skips the rest of the data object
       header.
     */
    id3->first_frame_offset = lseek(fd, 0, SEEK_CUR) + 26;
    id3->filesize = filesize(fd);
    /* We copy the wfx struct to the MP3 TOC field in the id3 struct so
       the codec doesn't need to parse the header object again */
    memcpy(id3->toc, &wfx, sizeof(wfx));

    return true;
}
