/*
 * 072-frames-xml.h (issue 703) - the frames the interface's XML makes, built in the frame tree
 *
 * For a general audience: 066-framexml reads the interface's XML files
 * and works out, for each frame they make, what the XML says about it
 * (its name, parent, size, anchors, layers, children) with its templates
 * applied. This turns each of those into a real frame or region in the
 * frame tree (071-frames), so it can be laid out and drawn: the size is
 * set, the anchors tied, the strata, level, scale and hidden flag set,
 * each region put in its draw layer, and then its children built, in
 * the order 066 made them.
 *
 * What the XML says, and what is done with it (issue 703's Decisions
 * give the reasons):
 *   - <Size x y> or <Size><AbsDimension x y/></Size>, in load order: each
 *     sets what it gives (x the width, y the height).
 *   - Anchors as 066 gives them: tied to the object they name (a frame
 *     made from XML, or one made by the scripts found by name), to the
 *     parent when relativeTo is left out, to the screen with no parent.
 *     One that can't be tied (its frame isn't in the tree) is a warning
 *     and skipped. setAllPoints="true" ties all points to the parent.
 *   - A region written as a named part of its frame (NormalTexture,
 *     HighlightTexture ...) with no anchors is tied to all points of its
 *     frame; ButtonText with no anchors to the frame's centre.
 *   - hidden="true" hides it; scale, frameStrata, clampedToScreen set
 *     those; frameLevel sets the level (without the one-call raise
 *     limit), except that 0 or less is refused with a warning (a quirk;
 *     FramesXmlOptions.level_zero corrects it to accept 0).
 *   - A region inside <Layers> is in its <Layer level>'s draw layer
 *     (ARTWORK when not given) at its textureSubLevel (0 when not given);
 *     a named part is in ARTWORK, except HighlightTexture in HIGHLIGHT.
 *   - Fonts (<Font>) are not objects of the tree, and are skipped.
 *
 * Each warning goes to FramesXmlOptions.warn with the file and line of
 * the element it is about, and is counted.
 */
#ifndef W_FRAMES_XML_H
#define W_FRAMES_XML_H

#include "013-errors.h"
#include "066-framexml.h"
#include "071-frames.h"

/* How a builder behaves, field by field (all zero is fine):
 *   level_zero  the correction for the quirk "XML level 0" (703): 0, a
 *       frameLevel of 0 or less is refused with a warning; 1, 0 is
 *       accepted (below 0 is still refused)
 *   ctx         handed back to `warn`
 *   warn        each warning as it happens: the file, the line (0 when
 *       none), the sentence. NULL: they are only counted */
typedef struct {
    int level_zero;
    void *ctx;
    void (*warn)(void *ctx, const char *file, int line, const char *sentence);
} FramesXmlOptions;

/* A builder (the handle: docs/001, "Handles") into the frame tree
 * `frames`, which it doesn't own: it keeps which tree object each XML
 * object became, and how far through an Fxml's steps it has built.
 * NULL when there is no memory, with `why`. */
typedef struct FramesXml FramesXml;
FramesXml *frames_xml_new(Frames *frames, const FramesXmlOptions *opt, WWhy *why);
void       frames_xml_free(FramesXml *x);

/* Builds one object 066 made (a top-level one, or one fxml_create made
 * for CreateFrame with a template) and everything inside it; its id into
 * *id (FRAMES_NONE for a font, or a region with no frame to go on). One
 * built already is not built again: its id is given. W_OK, warnings or
 * not; W_ERR_FULL when there is no memory. */
WErr frames_xml_build(FramesXml *x, const FxmlObject *o, int *id, WWhy *why);

/* Builds every object step of `fx` not built yet, in the order loading
 * made them (call it after each fxml_load). */
WErr frames_xml_catch_up(FramesXml *x, const Fxml *fx, WWhy *why);

/* The tree object an XML object became, or FRAMES_NONE. */
int frames_xml_id(const FramesXml *x, const FxmlObject *o);
int frames_xml_warning_count(const FramesXml *x);

#endif
