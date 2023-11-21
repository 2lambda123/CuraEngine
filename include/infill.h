// Copyright (c) 2023 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

#ifndef INFILL_H
#define INFILL_H

#include <numbers>

#include <range/v3/range/concepts.hpp>

#include "infill/LightningGenerator.h"
#include "infill/ZigzagConnectorProcessor.h"
#include "settings/EnumSettings.h" //For infill types.
#include "settings/Settings.h"
#include "settings/types/Angle.h"
#include "utils/AABB.h"
#include "utils/ExtrusionLine.h"
#include "utils/IntPoint.h"
#include "utils/section_type.h"

namespace cura
{

class SierpinskiFillProvider;
class SliceMeshStorage;

class Infill
{
    friend class InfillTest;

    EFillMethod pattern_{}; //!< the space filling pattern of the infill to generate
    bool zig_zaggify_{}; //!< Whether to connect the end pieces of the support lines via the wall
    bool connect_lines_{ calcConnectLines(pattern_, zig_zaggify_) }; //!< Whether the lines and zig_zaggification are generated by the connectLines algorithm
    // TODO: The connected lines algorithm is only available for linear-based infill, for now.
    // We skip ZigZag, Cross and Cross3D because they have their own algorithms. Eventually we want to replace all that with the new algorithm.
    // Cubic Subdivision ends lines in the center of the infill so it won't be effective.
    bool connect_polygons_{}; //!< Whether to connect as much polygons together into a single path
    Polygons outer_contour_{}; //!< The area that originally needs to be filled with infill. The input of the algorithm.
    Polygons inner_contour_{}; //!< The part of the contour that will get filled with an infill pattern. Equals outer_contour minus the extra infill walls.
    coord_t infill_line_width_{}; //!< The line width of the infill lines to generate
    coord_t line_distance_{}; //!< The distance between two infill lines / polygons
    coord_t infill_overlap_{}; //!< the distance by which to overlap with the actual area within which to generate infill
    size_t infill_multiplier_{}; //!< the number of infill lines next to each other
    AngleDegrees fill_angle_{}; //!< for linear infill types: the angle of the infill lines (or the angle of the grid)
    coord_t z_{}; //!< height of the layer for which we generate infill
    coord_t shift_{}; //!< shift of the scanlines in the direction perpendicular to the fill_angle
    coord_t max_resolution_{}; //!< Min feature size of the output
    coord_t max_deviation_{}; //!< Max deviation fro the original poly when enforcing max_resolution
    size_t wall_line_count_{}; //!< Number of walls to generate at the boundary of the infill region, spaced \ref infill_line_width apart
    coord_t small_area_width_{}; //!< Maximum width of a small infill region to be filled with walls
    Point infill_origin_{}; //!< origin of the infill pattern
    bool skip_line_stitching_{ false }; //!< Whether to bypass the line stitching normally performed for polyline type infills
    bool fill_gaps_{ true }; //!< Whether to fill gaps in strips of infill that would be too thin to fit the infill lines. If disabled, those areas are left empty.
    bool connected_zigzags_{ false }; //!< (ZigZag) Whether endpieces of zigzag infill should be connected to the nearest infill line on both sides of the zigzag connector
    bool use_endpieces_{ false }; //!< (ZigZag) Whether to include endpieces: zigzag connector segments from one infill line to itself
    bool skip_some_zags_{ false }; //!< (ZigZag) Whether to skip some zags
    size_t zag_skip_count_{}; //!< (ZigZag) To skip one zag in every N if skip some zags is enabled
    coord_t pocket_size_{}; //!< The size of the pockets at the intersections of the fractal in the cross 3d pattern
    bool mirror_offset_{}; //!< Indication in which offset direction the extra infill lines are made

    static constexpr auto one_over_sqrt_2 = 1.0 / std::numbers::sqrt2;

    constexpr bool calcConnectLines(const EFillMethod pattern, const bool zig_zaggify)
    {
        return zig_zaggify
            && (pattern == EFillMethod::LINES || pattern == EFillMethod::TRIANGLES || pattern == EFillMethod::GRID || pattern == EFillMethod::CUBIC
                || pattern == EFillMethod::TETRAHEDRAL || pattern == EFillMethod::QUARTER_CUBIC || pattern == EFillMethod::TRIHEXAGON);
    }

public:
    Infill() noexcept = default;

    Infill(
        EFillMethod pattern,
        bool zig_zaggify,
        bool connect_polygons,
        Polygons in_outline,
        coord_t infill_line_width,
        coord_t line_distance,
        coord_t infill_overlap,
        size_t infill_multiplier,
        AngleDegrees fill_angle,
        coord_t z,
        coord_t shift,
        coord_t max_resolution,
        coord_t max_deviation) noexcept
        : pattern_{ pattern }
        , zig_zaggify_{ zig_zaggify }
        , connect_polygons_{ connect_polygons }
        , outer_contour_{ in_outline }
        , infill_line_width_{ infill_line_width }
        , line_distance_{ line_distance }
        , infill_overlap_{ infill_overlap }
        , infill_multiplier_{ infill_multiplier }
        , fill_angle_{ fill_angle }
        , z_{ z }
        , shift_{ shift }
        , max_resolution_{ max_resolution }
        , max_deviation_{ max_deviation }
    {
    }

    Infill(
        EFillMethod pattern,
        bool zig_zaggify,
        bool connect_polygons,
        Polygons in_outline,
        coord_t infill_line_width,
        coord_t line_distance,
        coord_t infill_overlap,
        size_t infill_multiplier,
        AngleDegrees fill_angle,
        coord_t z,
        coord_t shift,
        coord_t max_resolution,
        coord_t max_deviation,
        size_t wall_line_count,
        coord_t small_area_width,
        Point infill_origin,
        bool skip_line_stitching) noexcept
        : pattern_{ pattern }
        , zig_zaggify_{ zig_zaggify }
        , connect_polygons_{ connect_polygons }
        , outer_contour_{ in_outline }
        , infill_line_width_{ infill_line_width }
        , line_distance_{ line_distance }
        , infill_overlap_{ infill_overlap }
        , infill_multiplier_{ infill_multiplier }
        , fill_angle_{ fill_angle }
        , z_{ z }
        , shift_{ shift }
        , max_resolution_{ max_resolution }
        , max_deviation_{ max_deviation }
        , wall_line_count_{ wall_line_count }
        , small_area_width_{ small_area_width }
        , infill_origin_{ infill_origin }
        , skip_line_stitching_{ skip_line_stitching }
    {
    }

    Infill(
        EFillMethod pattern,
        bool zig_zaggify,
        bool connect_polygons,
        Polygons in_outline,
        coord_t infill_line_width,
        coord_t line_distance,
        coord_t infill_overlap,
        size_t infill_multiplier,
        AngleDegrees fill_angle,
        coord_t z,
        coord_t shift,
        coord_t max_resolution,
        coord_t max_deviation,
        size_t wall_line_count,
        coord_t small_area_width,
        Point infill_origin,
        bool skip_line_stitching,
        bool fill_gaps,
        bool connected_zigzags,
        bool use_endpieces,
        bool skip_some_zags,
        size_t zag_skip_count,
        coord_t pocket_size) noexcept
        : pattern_{ pattern }
        , zig_zaggify_{ zig_zaggify }
        , connect_polygons_{ connect_polygons }
        , outer_contour_{ in_outline }
        , infill_line_width_{ infill_line_width }
        , line_distance_{ line_distance }
        , infill_overlap_{ infill_overlap }
        , infill_multiplier_{ infill_multiplier }
        , fill_angle_{ fill_angle }
        , z_{ z }
        , shift_{ shift }
        , max_resolution_{ max_resolution }
        , max_deviation_{ max_deviation }
        , wall_line_count_{ wall_line_count }
        , small_area_width_{ small_area_width }
        , infill_origin_{ infill_origin }
        , skip_line_stitching_{ skip_line_stitching }
        , fill_gaps_{ fill_gaps }
        , connected_zigzags_{ connected_zigzags }
        , use_endpieces_{ use_endpieces }
        , skip_some_zags_{ skip_some_zags }
        , zag_skip_count_{ zag_skip_count }
        , pocket_size_{ pocket_size }
        , mirror_offset_{ zig_zaggify }
    {
    }

    /*!
     * Generate the infill.
     *
     * \param toolpaths (output) The resulting variable-width paths (from the extra walls around the pattern). Binned by inset_idx.
     * \param result_polygons (output) The resulting polygons (from concentric infill)
     * \param result_lines (output) The resulting line segments (from linear infill types)
     * \param settings A settings storage to use for generating variable-width walls.
     * \param cross_fill_provider Any pre-computed cross infill pattern, if the Cross or Cross3D pattern is selected.
     * \param mesh A mesh for which to generate infill (should only be used for non-helper-mesh objects).
     * \param[in] cross_fill_provider The cross fractal subdivision decision functor
     */
    void generate(
        std::vector<VariableWidthLines>& toolpaths,
        Polygons& result_polygons,
        Polygons& result_lines,
        const Settings& settings,
        int layer_idx,
        SectionType section_type,
        const std::shared_ptr<SierpinskiFillProvider>& cross_fill_provider = nullptr,
        const std::shared_ptr<LightningLayer>& lightning_layer = nullptr,
        const SliceMeshStorage* mesh = nullptr,
        const Polygons& prevent_small_exposed_to_air = Polygons());

    /*!
     * Generate the wall toolpaths of an infill area. It will return the inner contour and set the inner-contour.
     * This function is called within the generate() function but can also be called stand-alone
     *
     * \param toolpaths [out] The generated toolpaths. Binned by inset_idx.
     * \param outer_contour [in,out] the outer contour, this is offsetted with the infill overlap
     * \param wall_line_count [in] The number of walls that needs to be generated
     * \param line_width [in] The optimum wall line width of the walls
     * \param infill_overlap [in] The overlap of the infill
     * \param settings [in] A settings storage to use for generating variable-width walls.
     * \return The inner contour of the wall toolpaths
     */
    static Polygons generateWallToolPaths(
        std::vector<VariableWidthLines>& toolpaths,
        Polygons& outer_contour,
        const size_t wall_line_count,
        const coord_t line_width,
        const coord_t infill_overlap,
        const Settings& settings,
        int layer_idx,
        SectionType section_type);

private:
    struct InfillLineSegment
    {
        /*!
         * Creates a new infill line segment.
         *
         * The previous and next line segments will not yet be connected. You
         * have to set those separately.
         * \param start Where the line segment starts.
         * \param end Where the line segment ends.
         */
        InfillLineSegment(const Point start, const size_t start_segment, const size_t start_polygon, const Point end, const size_t end_segment, const size_t end_polygon)
            : start_(start)
            , altered_start_(start)
            , start_segment_(start_segment)
            , start_polygon_(start_polygon)
            , end_(end)
            , altered_end_(end)
            , end_segment_(end_segment)
            , end_polygon_(end_polygon)
            , previous_(nullptr)
            , next_(nullptr)
        {
        }

        /*!
         * Where the line segment starts.
         */
        Point start_;

        /*!
         * If the line-segment starts at a different point due to prevention of crossing near the boundary, it gets saved here.
         *
         * The original start-point is still used to determine ordering then, so it can't just be overwritten.
         */
        Point altered_start_;

        /*!
         * Which polygon line segment the start of this infill line belongs to.
         *
         * This is an index of a vertex in the PolygonRef that this infill line
         * is inside. It is used to disambiguate between the start and end of
         * the line segment.
         */
        size_t start_segment_;

        /*!
         * Which polygon the start of this infill line belongs to.
         *
         * This is an index of a PolygonRef that this infill line
         * is inside. It is used to know which polygon the start segment belongs to.
         */
        size_t start_polygon_;

        /*!
         * If the line-segment needs to prevent crossing with another line near its start, a point is inserted near the start.
         */
        std::optional<Point> start_bend_;

        /*!
         * Where the line segment ends.
         */
        Point end_;

        /*!
         * If the line-segment ends at a different point due to prevention of crossing near the boundary, it gets saved here.
         *
         * The original end-point is still used to determine ordering then, so it can't just be overwritten.
         */
        Point altered_end_;

        /*!
         * Which polygon line segment the end of this infill line belongs to.
         *
         * This is an index of a vertex in the PolygonRef that this infill line
         * is inside. It is used to disambiguate between the start and end of
         * the line segment.
         */
        size_t end_segment_;

        /*!
         * Which polygon the end of this infill line belongs to.
         *
         * This is an index of a PolygonRef that this infill line
         * is inside. It is used to know which polygon the end segment belongs to.
         */
        size_t end_polygon_;

        /*!
         * If the line-segment needs to prevent crossing with another line near its end, a point is inserted near the end.
         */
        std::optional<Point> end_bend_;

        /*!
         * The previous line segment that this line segment is connected to, if
         * any.
         */
        InfillLineSegment* previous_;

        /*!
         * The next line segment that this line segment is connected to, if any.
         */
        InfillLineSegment* next_;

        /*!
         * Compares two infill line segments for equality.
         *
         * This is necessary for putting line segments in a hash set.
         * \param other The line segment to compare this line segment with.
         */
        bool operator==(const InfillLineSegment& other) const;

        /*!
         * Invert the direction of the line-segment.
         *
         * Useful when the next move is from end to start instead of 'forwards'.
         */
        void swapDirection();

        /*!
         * Append this line-segment to the results, start, bends and end.
         *
         * \param include_start Wether to include the start point or not, useful when tracing a poly-line.
         */
        void appendTo(PolygonRef& result_polyline, const bool include_start = true);
    };

    /*!
     * Stores the infill lines (a vector) for each line of a polygon (a vector)
     * for each polygon in a Polygons object that we create a zig-zaggified
     * infill pattern for.
     */
    std::vector<std::vector<std::vector<InfillLineSegment*>>> crossings_on_line_;

    /*!
     * Generate the infill pattern without the infill_multiplier functionality
     */
    void _generate(
        std::vector<VariableWidthLines>& toolpaths,
        Polygons& result_polygons,
        Polygons& result_lines,
        const Settings& settings,
        const std::shared_ptr<SierpinskiFillProvider>& cross_fill_pattern = nullptr,
        const std::shared_ptr<LightningLayer>& lightning_layer = nullptr,
        const SliceMeshStorage* mesh = nullptr);

    /*!
     * Multiply the infill lines, so that any single line becomes [infill_multiplier] lines next to each other.
     *
     * This is done in a way such that there is not overlap between the lines
     * except the middle original one if the multiplier is odd.
     *
     * This introduces a lot of line segments.
     *
     * \param[in,out] result_polygons The polygons to be multiplied (input and output)
     * \param[in,out] result_lines The lines to be multiplied (input and output)
     */
    void multiplyInfill(Polygons& result_polygons, Polygons& result_lines);

    /*!
     * Generate gyroid infill
     * \param result_polylines (output) The resulting polylines
     * \param result_polygons (output) The resulting polygons, if zigzagging accidentally happened to connect gyroid lines in a circle.
     */
    void generateGyroidInfill(Polygons& result_polylines, Polygons& result_polygons);

    /*!
     * Generate lightning fill aka minfill aka 'Ribbed Support Vault Infill', see Tricard,Claux,Lefebvre/'Ribbed Support Vaults for 3D Printing of Hollowed Objects'
     * see https://hal.archives-ouvertes.fr/hal-02155929/document
     * \param result (output) The resulting polygons
     */
    void generateLightningInfill(const std::shared_ptr<LightningLayer>& lightning_layer, Polygons& result_lines);

    /*!
     * Generate sparse concentric infill
     *
     * \param toolpaths (output) The resulting toolpaths. Binned by inset_idx.
     * \param inset_value The offset between each consecutive two polygons
     */
    void generateConcentricInfill(std::vector<VariableWidthLines>& toolpaths, const Settings& settings);

    /*!
     * Generate a rectangular grid of infill lines
     * \param[out] result (output) The resulting lines
     */
    void generateGridInfill(Polygons& result);

    /*!
     * Generate a shifting triangular grid of infill lines, which combine with consecutive layers into a cubic pattern
     * \param[out] result (output) The resulting lines
     */
    void generateCubicInfill(Polygons& result);

    /*!
     * Generate a double shifting square grid of infill lines, which combine with consecutive layers into a tetrahedral pattern
     * \param[out] result (output) The resulting lines
     */
    void generateTetrahedralInfill(Polygons& result);

    /*!
     * Generate a double shifting square grid of infill lines, which combine with consecutive layers into a quarter cubic pattern
     * \param[out] result (output) The resulting lines
     */
    void generateQuarterCubicInfill(Polygons& result);

    /*!
     * Generate a single shifting square grid of infill lines.
     * This is used in tetrahedral infill (Octet infill) and in Quarter Cubic infill.
     *
     * \param pattern_z_shift The amount by which to shift the whole pattern down
     * \param angle_shift The angle to add to the infill_angle
     * \param[out] result (output) The resulting lines
     */
    void generateHalfTetrahedralInfill(double pattern_z_shift, int angle_shift, Polygons& result);

    /*!
     * Generate a triangular grid of infill lines
     * \param[out] result (output) The resulting lines
     */
    void generateTriangleInfill(Polygons& result);

    /*!
     * Generate a triangular grid of infill lines
     * \param[out] result (output) The resulting lines
     */
    void generateTrihexagonInfill(Polygons& result);

    /*!
     * Generate a 3d pattern of subdivided cubes on their points
     * \param[out] result The resulting lines
     * \param[in] mesh Where the Cubic Subdivision Infill precomputation is stored
     */
    void generateCubicSubDivInfill(Polygons& result, const SliceMeshStorage& mesh);

    /*!
     * Generate a 3d pattern of subdivided cubes on their points
     * \param[in] cross_fill_provider Where the cross fractal precomputation is stored
     * \param[out] result_polygons The resulting polygons
     * \param[out] result_lines The resulting lines
     */
    void generateCrossInfill(const SierpinskiFillProvider& cross_fill_provider, Polygons& result_polygons, Polygons& result_lines);

    /*!
     * Convert a mapping from scanline to line_segment-scanline-intersections (\p cut_list) into line segments, using the even-odd rule
     * \param[out] result (output) The resulting lines
     * \param rotation_matrix The rotation matrix (un)applied to enforce the angle of the infill
     * \param scanline_min_idx The lowest index of all scanlines crossing the polygon
     * \param line_distance The distance between two lines which are in the same direction
     * \param boundary The axis aligned boundary box within which the polygon is
     * \param cut_list A mapping of each scanline to all y-coordinates (in the space transformed by rotation_matrix) where the polygons are crossing the scanline
     * \param total_shift total shift of the scanlines in the direction perpendicular to the fill_angle.
     */
    void addLineInfill(
        Polygons& result,
        const PointMatrix& rotation_matrix,
        const int scanline_min_idx,
        const int line_distance,
        const AABB boundary,
        std::vector<std::vector<coord_t>>& cut_list,
        coord_t total_shift);

    /*!
     * generate lines within the area of \p in_outline, at regular intervals of \p line_distance
     *
     * idea:
     * intersect a regular grid of 'scanlines' with the area inside \p in_outline
     *
     * \param[out] result (output) The resulting lines
     * \param line_distance The distance between two lines which are in the same direction
     * \param infill_rotation The angle of the generated lines
     * \param extra_shift extra shift of the scanlines in the direction perpendicular to the infill_rotation
     */
    void generateLineInfill(Polygons& result, int line_distance, const double& infill_rotation, coord_t extra_shift);

    /*!
     * Function for creating linear based infill types (Lines, ZigZag).
     *
     * This function implements the basic functionality of Infill::generateLineInfill (see doc of that function),
     * but makes calls to a ZigzagConnectorProcessor which handles what to do with each line segment - scanline intersection.
     *
     * It is called only from Infill::generateLineinfill and Infill::generateZigZagInfill.
     *
     * \param[out] result (output) The resulting lines
     * \param line_distance The distance between two lines which are in the same direction
     * \param rotation_matrix The rotation matrix (un)applied to enforce the angle of the infill
     * \param zigzag_connector_processor The processor used to generate zigzag connectors
     * \param connected_zigzags Whether to connect the endpiece zigzag segments on both sides to the same infill line
     * \param extra_shift extra shift of the scanlines in the direction perpendicular to the fill_angle
     */
    void generateLinearBasedInfill(
        Polygons& result,
        const int line_distance,
        const PointMatrix& rotation_matrix,
        ZigzagConnectorProcessor& zigzag_connector_processor,
        const bool connected_zigzags,
        coord_t extra_shift);

    /*!
     *
     * generate lines within the area of [in_outline], at regular intervals of [line_distance]
     * idea:
     * intersect a regular grid of 'scanlines' with the area inside [in_outline] (see generateLineInfill)
     * zigzag:
     * include pieces of boundary, connecting the lines, forming an accordion like zigzag instead of separate lines    |_|^|_|
     *
     * Note that ZigZag consists of 3 types:
     * - without endpieces
     * - with disconnected endpieces
     * - with connected endpieces
     *
     *     <--
     *     ___
     *    |   |   |
     *    |   |   |
     *    |   |___|
     *         -->
     *
     *        ^ = even scanline
     *  ^            ^ no endpieces
     *
     * start boundary from even scanline! :D
     *
     *
     *                 v  disconnected end piece: leave out last line segment
     *          _____
     *   |     |     |  \                     .
     *   |     |     |  |
     *   |_____|     |__/
     *
     *   ^     ^     ^    scanlines
     *
     *
     *                 v  connected end piece
     *          ________
     *   |     |     |  \                      .
     *   |     |     |  |
     *   |_____|     |__/                       .
     *
     *   ^     ^     ^    scanlines
     *
     * \param[out] result (output) The resulting lines
     * \param line_distance The distance between two lines which are in the same direction
     * \param infill_rotation The angle of the generated lines
     */
    void generateZigZagInfill(Polygons& result, const coord_t line_distance, const double& infill_rotation);

    /*!
     * determine how far the infill pattern should be shifted based on the values of infill_origin and \p infill_rotation
     *
     * \param[in] infill_rotation the angle the infill pattern is rotated through
     *
     * \return the distance the infill pattern should be shifted
     */
    coord_t getShiftOffsetFromInfillOriginAndRotation(const double& infill_rotation);

    /*!
     * Used to prevent intersections of linear-based infill.
     *
     * When connecting infill, and the infill crosses itself near the boundary, small 'loops' can occur, which have large internal angles.
     * Prevent this by altering the two crossing line-segments just before the crossing takes place:
     *
     *  \   /    \   /
     *   \ /      \ /
     *    X       | |
     *   / \      | |
     *   ---       -
     * =======  =======
     *  before   after
     *
     * \param at_distance At which distance the offset of the bisector takes place (will be the length of the resulting connection along the edge).
     * \param intersect The point at which these line-segments intersect.
     * \param connect_start Input; the original point at the border which the first line-segment touches. Output; the updated point.
     * \param connect_end Input; the original point at the border which the second line-segment touches. Output; the updated point.
     * \param a The first line-segment.
     * \param b The second line-segment.
     */
    void resolveIntersection(const coord_t at_distance, const Point& intersect, Point& connect_start, Point& connect_end, InfillLineSegment* a, InfillLineSegment* b);

    /*!
     * Connects infill lines together so that they form polylines.
     *
     * In most cases it will end up with only one long line that is more or less
     * optimal. The lines are connected on their ends by extruding along the
     * border of the infill area, similar to the zigzag pattern.
     * \param[in/out] result_lines The lines to connect together.
     */
    void connectLines(Polygons& result_lines);
};
static_assert(concepts::semiregular<Infill>, "Infill should be semiregular");

} // namespace cura

#endif // INFILL_H
