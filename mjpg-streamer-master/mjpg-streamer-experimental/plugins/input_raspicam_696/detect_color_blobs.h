/*
 * Copyright (C) 2017 by Daniel Clouse.
 *
 * This file is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 *
 *  a) This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful, 
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this library; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *     MA 02110-1301 USA
 *
 * Alternatively,
 *
 *  b) Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file detect_color_blobs.h
 * Detect color blobs in a YUV image.
 */
#ifndef DETECT_COLOR_BLOBS_H
#define DETECT_COLOR_BLOBS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief Index into the blob_set array of a Blob_List.
 *
 * We use indices instead of pointers to save space and thus increase cache
 * hits in the major data structures, which are arrays of Blob_Set_Entrys and
 * Yuv_Runs.  For example, the number of bytes in a Yuv_Run is only 6 using
 * this representation; whereas with a pointer representation 16 bytes would
 * be required.  Hence a 64 byte cache line holds 10 Yuv_Runs instead of just 4,
 * increasing locality of reference considerably.
 */
typedef unsigned short Blob_Set_Index;

/** Index into the root_info array of a Blob_List. */
#define NOT_A_ROOT_LIST_INDEX USHRT_MAX
typedef unsigned short Root_List_Index;

/**
 * @brief A short statistical description of a color blob.
 *
 * All statistics are designed to be calculable in rolling fashion.  I.e.
 * each time you add to the blob, you can update these statistics easily.
 * Includes a bounding box, and intermediate results for calculation of the
 * center of mass.
 */
typedef struct {
    unsigned short min_x;  /// left pixel of bbox
    unsigned short max_x;  /// right pixel of bbox
    unsigned short min_y;  /// top pixel of bbox
    unsigned short max_y;  /// bottom pixel of bbox
    unsigned long sum_x;   /// sum of all x-coords of all pixels in the blob
    unsigned long sum_y;   /// sum of all y-coords of all pixels in the blob
    unsigned int count;    /// the number of pixels in the blob.
} Blob_Stats;


/**
 * @brief One element in the disjoint set forest (blob_set field of Blob_List).
 *
 * See the discussion of the blob_set field of a Blob_List.
 */
typedef struct {
    Blob_Set_Index parent_index;     /// index into blob_set array
    Root_List_Index root_list_index; /// index into root_info array
    /** Approximates the log of the number of items rooted at this node. */
    unsigned short rank;
} Blob_Set_Entry;

typedef struct {
    Blob_Stats stats;         /// describes entire blob rooted here
    Blob_Set_Index set_index; /// index to root entry in blob_set_array
} Root_Info;


/**
 * @brief Represents all the color blobs found in an image.
 */
typedef struct {
    /** @brief The blob_set field is an array of Blob_Set_Entries that
               contains a representation of all the pixel blobs found so far in
               the image.
               
       These are stored using a modified version of the disjoint set forest
       representation as described by Cormen, Leiserson and Rivest,
       "Introduction to Algorithms," 1990, Section 22.3.  In this modified
       version, as soon as a child Blob_Set_Entry is linked to a parent
       Blob_Set_Entry, statistics about the pixels represented by the child are
       merged into the parent entry's statistics, and the child entry's
       statistics cease to be meaningful.  Effectively, all the statistics for
       the entire blob are always stored in the root of the blob.  To reduce
       the number of Blob_Set_Entries, note that a Blob_Set_Entry is not
       created until the blob occurs in at least two rows of the image.  (Blobs
       that are only one pixel high are never represented.  Blobs for which a
       root is immediately available are only represented in statistics, not
       in an actual Blob_Set_Entry.)
       
       Entries are never reused or deleted from this array.  The next free
       entry is at &blob_set[used_blob_set_count]. */
    Blob_Set_Entry* blob_set;
    Blob_Set_Index max_blob_set_count;  /// number of elements in blob_set array
    Blob_Set_Index used_blob_set_count; /// used element in blob_set array

    /** @brief The root_info field contains a list of indices into the
               blob_set array, one for each Blob_Set_Entry that is the root of
               its own disjoint set.
               
       This allows all the roots to be easily found at the end of processing.
       The list is stored as a densely packed array.  The next available empty
       entry of root_info is always at &root_info[used_root_list_count]. */

    //Blob_Set_Index* root_list;
    Root_Info* root_info;
    Blob_Set_Index max_root_list_count;/// number of elements in root_info array
    Blob_Set_Index used_root_list_count; /// used element in root_info array
} Blob_List;


/**
 * @brief Allocate memory for a Blob_List and initialize it to empty.
 *
 * @param max_runs [in]  The maximum number of color runs in all color blobs.
 *                       A run is a dense sequence of pixels within a single
 *                       image row that are all in the same color blob.
 * @param max_blobs [in] The maximum number of color blobs this Blob_List can
 *                       represent.
 * @return The newly allocated Blob_List.
 */
Blob_List blob_list_init(Blob_Set_Index max_runs, Blob_Set_Index max_blobs);


/**
 * @brief Empty the given Blob_List so that it is returned to pristine state.
 *
 * @param p [in,out]  A pointer to a Blob_List, as returned from
 *                    blob_list_init().
 */
void blob_list_clear(Blob_List* p);


/**
 * @brief Free all memory captured by the given Blob_List.
 *
 * @param p [in,out]  A pointer to a Blob_List, as returned from
 *                    blob_list_init().
 */
void blob_list_deinit(Blob_List* p);


/**
 * @brief Detect color blobs within the given YUV420 image.
 *
 * A blob is a contiguous set of 4-neighbors all with color values (YUV) in
 * which Y >= y_low and U >= u_low and U <= u_high and V >= v_low and
 * V <= v_high.
 *
 * @param p [in,out]                     A pointer to a Blob_List, as returned
 *                                       from blob_list_init().  This routine
 *                                       first clears the Blob_List, then adds
 *                                       in new detected blobs from the given
 *                                       image.
 * @param y_low [in]                     The minimum acceptable Y color value
 *                                       for a pixel to considered part of a
 *                                       blob.  Note that there is no y_high.
 *                                       Supporting y_high slows the code by
 *                                       about 20 per cent.  Since it isn't
 *                                       useful for our application, we left it
 *                                       out.
 * @param u_low [in]                     The minimum U color value.
 * @param u_high [in]                    The maximum U color value.
 * @param v_low [in]                     The minimum V color value.
 * @param v_high [in]                    The maximum V color value.
 * @param highlight_detected_pixels [in] True means change the value of each
 *                                       pixel in the image that is a part of a
 *                                       color blob, so as to highlight its
 *                                       specialness.  This takes extra time,
 *                                       so don't turn this on if you want to
 *                                       run fast.
 * @param cols [in]                      The number of columns in the given
 *                                       image.  More specifically, the number
 *                                       of columns in the Y component of the
 *                                       given image.
 * @param rows [in]                      Likewise, the number of rows in the
 *                                       given image.
 * @param yuv [in,out]                   Contains YUV color values for each
 *                                       pixel in the image in YUV420 order.
 *                                       The first cols * rows values of this
 *                                       array contain Y values in row major
 *                                       order.  This is followed by an array
 *                                       of (cols/2 * rows/2) U values, each
 *                                       entry representing a 2x2 block of
 *                                       pixels in row major order.  This is
 *                                       followed by V values similarly
 *                                       arranged.  If do_mark_detected_pixels
 *                                       is true, the image is altered by this
 *                                       routine to highlight pixels that meet
 *                                       color thresholds specified by y_low,
 *                                       u_low, etc.  Otherwise, the image is
 *                                       left unchanged.
 */
void detect_color_blobs(Blob_List* p,
                        unsigned char y_low,
                        unsigned char u_low,
                        unsigned char u_high,
                        unsigned char v_low,
                        unsigned char v_high,
                        bool highlight_detected_pixels,
                        int cols,
                        int rows,
                        unsigned char yuv[]);

#if 0
/**
 * @brief Return statistics on the iTH color blob in the given Blob_List.
 *
 * @param p [in]       A pointer to a Blob_List, as returned from
 *                     detect_color_blobs().
 * @param i [in]       The index of the blob of interest.
 * @return A pointer to the statistics for the specified blob.
 */
static inline const Blob_Stats* get_blob_stats(const Blob_List* p, int i) {
    //return &p->blob_set[p->root_list[i]].stats;
    return &root_info[i].stats;
}
#endif

/**
 * @brief Return the count of detected pixels across all blobs.
 */
unsigned long get_total_blob_pixel_count(Blob_List* p);

/**
 * @brief Sort the blobs contained in the root_info array of the given
 *        Blob_List into declining pixel count order.
 *
 * After this call, the largest blob will be found in p->root_info[0], second
 * largest in p->root_info[1], etc.
 * @param p [in,out]   A pointer to a Blob_List, as returned from
 *                     detect_color_blobs().
 */
void sort_blobs_by_pixel_count(Blob_List* p);

unsigned int blob_list_purge_small_bboxes(Blob_List* p,
                                          unsigned int min_pixels_per_blob);

unsigned int copy_best_bounding_boxes(Blob_List* p,
                                      int bbox_element_count,
                                      unsigned short bbox_element[]);

unsigned int copy_best_bboxes_to_blob_stats_array(Blob_List* p,
                                                  int stats_count,
                                                  Blob_Stats stats[]);

/**
 * @brief Draw bounding boxes around detected blobs in the given image.
 *
 * @param p [in,out]                A pointer to a Blob_List, as returned from
 *                                  detect_color_blobs().
 * @param min_pixels_per_blob [in]  Bounding boxes will not be drawn for Blobs
 *                                  containing fewer than this many pixels.
 * @param bbox_color_yuv [in]       The color to use in drawing bounding boxes.
 * @param cols [in]                 The number of columns in the image.
 * @param rows [in]                 The number of rows in the image.
 * @param yuv [in,out]              The image.  See detect_color_blobs() for
 *                                  details on the representation used.
 */
void draw_bounding_boxes(const Blob_List* p,
                         int min_pixels_per_blob,
                         const unsigned char bbox_color_yuv[3],
                         int cols,
                         int rows,
                         unsigned char yuv[]);
#ifdef __cplusplus
}
#endif
#endif
