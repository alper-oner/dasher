#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <cspi/spi.h>

#include <X11/keysym.h>
#include <algorithm>

#include "dasher_editor_external.h"
#include "dasher_editor_private.h"
#include "dasher_lock_dialogue.h"
#include "dasher_main.h"
#include "../DasherCore/ControlManager.h"

void dasher_editor_external_handle_focus(DasherEditor *pSelf, const AccessibleEvent *pEvent);
void dasher_editor_external_handle_caret(DasherEditor *pSelf, const AccessibleEvent *pEvent);

void focus_listener(const AccessibleEvent *pEvent, void *pUserData);
void caret_listener(const AccessibleEvent *pEvent, void *pUserData);

enum {
  NOT_INIT,
  INIT_SUCCESS,
  INIT_FAIL
}  status = NOT_INIT;

bool initSPI() {
  if (status == NOT_INIT) {
    status = (SPI_init()==2) ? INIT_FAIL : INIT_SUCCESS;
  }
  return (status==INIT_SUCCESS);
}

void
dasher_editor_external_create_buffer(DasherEditor *pSelf) {
  DasherEditorPrivate *pPrivate = DASHER_EDITOR_GET_PRIVATE(pSelf);


  if(!initSPI()) {
    g_message("Could not initialise SPI - accessibility options disabled");
  } else {
    pPrivate->pFocusListener = SPI_createAccessibleEventListener(focus_listener, pSelf);
    pPrivate->pCaretListener = SPI_createAccessibleEventListener(caret_listener, pSelf);
    
    // TODO: Need to deregister these on destruction
    
    if(pPrivate->pFocusListener && pPrivate->pCaretListener) {
      SPI_registerGlobalEventListener(pPrivate->pFocusListener, "focus:");
      SPI_registerGlobalEventListener(pPrivate->pCaretListener, "object:text-caret-moved");
    } else {
      g_message("Could not obtain an SPI listener");
    }
  }    

  pPrivate->pAccessibleText = 0;
}

void
dasher_editor_external_output(DasherEditor *pSelf, const gchar *szText, int iOffset /* unused */) {
  if(!initSPI())
    return;

  char *szNewText;
  szNewText = new char[strlen(szText) + 1];
  strcpy(szNewText, szText);
  
  SPI_generateKeyboardEvent(0, szNewText, SPI_KEY_STRING);
  
  delete[] szNewText;
}

void
dasher_editor_external_delete(DasherEditor *pSelf, int iLength, int iOffset) {
  if(!initSPI()) return;

  SPI_generateKeyboardEvent(XK_BackSpace, NULL, SPI_KEY_SYM);
}

const gchar *
dasher_editor_external_get_context(DasherEditor *pSelf, int iOffset, int iLength) {
  DasherEditorPrivate *pPrivate = DASHER_EDITOR_GET_PRIVATE(pSelf);
  if(pPrivate->pAccessibleText)
    return AccessibleText_getText(pPrivate->pAccessibleText, iOffset, iOffset + iLength);
  else
    return "";
}

gint
dasher_editor_external_get_offset(DasherEditor *pSelf) {
  DasherEditorPrivate *pPrivate = DASHER_EDITOR_GET_PRIVATE(pSelf);
  
  if(!pPrivate->pAccessibleText)
    return 0;
  if (AccessibleText_getNSelections(pPrivate->pAccessibleText)==0)
    return AccessibleText_getCaretOffset(pPrivate->pAccessibleText);
  long int start,end;
  AccessibleText_getSelection(pPrivate->pAccessibleText, 0, &start, &end);
  return std::min(start,end);
}

void dasher_editor_external_handle_focus(DasherEditor *pSelf, const AccessibleEvent *pEvent) {
  DasherEditorPrivate *pPrivate = DASHER_EDITOR_GET_PRIVATE(pSelf);

  //  g_message("Focus");
  
  if(pPrivate->pAccessibleText) {
    AccessibleText_unref(pPrivate->pAccessibleText);
    pPrivate->pAccessibleText = 0;
  }
  
  Accessible *accessible = pEvent->source;
  Accessible_ref(accessible);
  
  //  g_message("%s", Accessible_getName(accessible));
  // g_message("%s", Accessible_getRoleName(accessible));
  //g_message("%s", Accessible_getDescription(accessible));

  if(Accessible_isText(accessible) || Accessible_isEditableText(accessible)) {
    pPrivate->pAccessibleText = Accessible_getText(accessible);
    AccessibleText_ref(pPrivate->pAccessibleText);

//     g_iExpectedPosition = AccessibleText_getCaretOffset(pPrivate->pAccessibleText);
//     g_iOldPosition = g_iExpectedPosition;

    //ACL: in old code, external_buffer emitted signal, for which the editor_external had
    // registered a callback, which then emitted the same/corresponding signal from the
    // editor_external; so by combining buffer into editor, seems we don't need any of that
    // and can just emit the signal from the editor directly. However, the callback also said:
    //TODO: plumb signal back into control
    // ...if that makes any sense?

    g_signal_emit_by_name(G_OBJECT(pSelf), "buffer_changed", G_OBJECT(pSelf), NULL, NULL);

  }

  Accessible_unref(accessible);
}

void dasher_editor_external_handle_caret(DasherEditor *pSelf, const AccessibleEvent *pEvent) {
  //  g_message("Caret");
 DasherEditorPrivate *pPrivate = DASHER_EDITOR_GET_PRIVATE(pSelf);

 //  g_message("Focus");
  
  if(pPrivate->pAccessibleText) {
    AccessibleText_unref(pPrivate->pAccessibleText);
    pPrivate->pAccessibleText = 0;
  }
  
  Accessible *accessible = pEvent->source;
  Accessible_ref(accessible);
  
  // g_message("%s", Accessible_getName(accessible));
  //g_message("%s", Accessible_getRoleName(accessible));
  //g_message("%s", Accessible_getDescription(accessible));

  if(Accessible_isText(accessible) || Accessible_isEditableText(accessible)) {
    pPrivate->pAccessibleText = Accessible_getText(accessible);
    AccessibleText_ref(pPrivate->pAccessibleText);

//     g_iExpectedPosition = AccessibleText_getCaretOffset(pPrivate->pAccessibleText);
//     g_iOldPosition = g_iExpectedPosition;

    //ACL: in old code, dasher_external_buffer emitted offset_changed signal,
    // picked up by editor_external's external_context_changed_handler,
    // which just emitted a context_changed signal from the editor_external.
    // So, just emit that directly:
    g_signal_emit_by_name(G_OBJECT(pSelf), "context_changed", G_OBJECT(pSelf), NULL, NULL);
  }

  Accessible_unref(accessible);
//   Accessible_ref(event->source);

//   AccessibleStateSet *pAStateSet = Accessible_getStateSet(event->source);
//   AccessibleStateSet_ref(pAStateSet);

//   if(!AccessibleStateSet_contains(pAStateSet, SPI_STATE_FOCUSED)) {
//     return;
//   }

//   AccessibleStateSet_unref(pAStateSet);
//   Accessible_unref(event->source);
  
//   if(g_pAccessibleText) {
//     int iActualPosition = event->detail1;

//     std::cout << "Expected: " << g_iExpectedPosition << " Actual: " << iActualPosition << std::endl;

//     if((g_iExpectedPosition - iActualPosition) * (g_iOldPosition - iActualPosition) > 0) {
//       gtk_dasher_control_invalidate_context(GTK_DASHER_CONTROL(pDasherWidget));
//       g_iExpectedPosition = iActualPosition;
//     }

//     g_iOldPosition = iActualPosition;
//   }
}

void focus_listener(const AccessibleEvent *pEvent, void *pUserData) {
  dasher_editor_external_handle_focus((DasherEditor *)pUserData, pEvent);
}

void caret_listener(const AccessibleEvent *pEvent, void *pUserData) {
  dasher_editor_external_handle_caret((DasherEditor *)pUserData, pEvent);
}