/* App-owned module; assembled by apps/host/compose.h. */
/* LUMEN 1.0: native, paper-first applications, shared local persistence. */
#include "../flashcards/api.h"
#include "../../vendor/astronomy.h"
#include "../../vendor/ha_icon.h"
#include "../../vendor/sky_catalog.h"
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
static int v1_ready, v1_dirty, v1_page, v1_delete;
#include "../notes/folder_state.h"
#include "../todo/state.h"
#include "../flashcards/state.h"
#include "../wordle/state.h"
#include "../nonograms/shared_board_state.h"
#include "../photos/state.h"
#include "../news/archive_state.h"
#include "../launcher/state.h"
#include "../notes/markdown_state.h"
#include "../timer/presentation_state.h"
static int v1_word_index(void);
static void v1_photos_scan(void), v1_archive_scan(void), v1_nono_new(void);
