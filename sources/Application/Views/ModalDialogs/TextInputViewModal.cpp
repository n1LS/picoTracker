#include "TextInputViewModal.h"

#include "Application/AppWindow.h"
#include "Foundation/Constants/SpecialCharacters.h"
#include "MessageBox.h" // Include for button constants

bool TextInputViewModal::inUse_ = false;
alignas(TextInputViewModal) static unsigned char TextInputViewModalStorage
    [sizeof(TextInputViewModal)];
void *TextInputViewModal::storage_ = TextInputViewModalStorage;

TextInputViewModal *TextInputViewModal::Create(View &view, const char *title,
                                               const char *value,
                                               int maxTextLength) {
  if (inUse_) {
    auto *existing = reinterpret_cast<TextInputViewModal *>(storage_);
    existing->~TextInputViewModal();
    inUse_ = false;
  }
  if (inUse_) {
    auto *existing = reinterpret_cast<TextInputViewModal *>(storage_);
    existing->~TextInputViewModal();
    inUse_ = false;
  }
  inUse_ = true;
  return new (storage_) TextInputViewModal(view, title, value, maxTextLength);
}

void TextInputViewModal::Destroy() {
  this->~TextInputViewModal();
  inUse_ = false;
}

TextInputViewModal::TextInputViewModal(View &view, const char *title,
                                       const char *value, int maxTextLength)
    : ModalView(view), title_(title), maxTextLength_(maxTextLength) {
  strcpy(text_, value);
  editColumn_ = strlen(text_);
}

void TextInputViewModal::DrawView() {
  const int w = 26;
  const int h = 20;

  GUITextProperties props;
  SetColor(CD_NORMAL);

  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;

  // temp clear
  for (int dy = 1; dy < h - 1; dy++) {
    DrawString(x, y + dy, "                          ", props);
  }

  const char *chars = upperCase_ ? upperChars : lowerChars;
  char buffer[w];

  buffer[1] = 0;

  for (int dx = 0; dx < 11; dx++) {
    for (int dy = 0; dy < 4; dy++) {
      bool selected = (row_ == dy && col_ == dx);
      buffer[0] = chars[dx + dy * 11];
      props.invert_ = selected;
      SetColor(selected ? CD_HILITE2 : CD_NORMAL);
      DrawString(x + 2 + dx * 2, y + 7 + dy * 2, buffer, props);
    }
  }

  buffer[0] = char_border_single_vertical;
  for (int dy = 1; dy < h - 1; dy++) {
    DrawString(x, y + dy, buffer, props);
    DrawString(x + w - 1, y + dy, buffer, props);
  }

  DrawString(
      x, y,
      char_border_single_topLeft_s char_border_single_horizontal_s char_border_single_horizontal_s
          char_border_single_horizontal_s char_border_single_horizontal_s
              char_border_single_horizontal_s char_border_single_horizontal_s
                  char_border_single_horizontal_s char_border_single_horizontal_s
                      char_border_single_horizontal_s char_border_single_horizontal_s
                          char_border_single_horizontal_s char_border_single_horizontal_s
                              char_border_single_horizontal_s char_border_single_horizontal_s
                                  char_border_single_horizontal_s char_border_single_horizontal_s
                                      char_border_single_horizontal_s char_border_single_horizontal_s
                                          char_border_single_horizontal_s
                                              char_border_single_horizontal_s
                                                  char_border_single_horizontal_s
                                                      char_border_single_horizontal_s
                                                          char_border_single_horizontal_s
                                                              char_border_single_horizontal_s
                                                                  char_border_single_topRight_s,
      props);
  DrawString(
      x, y + 2,
      char_border_single_verticalRight_s char_border_single_horizontal_s char_border_single_horizontal_s
          char_border_single_horizontal_s char_border_single_horizontal_s
              char_border_single_horizontal_s char_border_single_horizontal_s
                  char_border_single_horizontal_s char_border_single_horizontal_s
                      char_border_single_horizontal_s char_border_single_horizontal_s
                          char_border_single_horizontal_s char_border_single_horizontal_s
                              char_border_single_horizontal_s char_border_single_horizontal_s
                                  char_border_single_horizontal_s char_border_single_horizontal_s
                                      char_border_single_horizontal_s char_border_single_horizontal_s
                                          char_border_single_horizontal_s
                                              char_border_single_horizontal_s
                                                  char_border_single_horizontal_s
                                                      char_border_single_horizontal_s
                                                          char_border_single_horizontal_s
                                                              char_border_single_horizontal_s
                                                                  char_border_single_verticalLeft_s,
      props);
  DrawString(
      x, y + 15,
      char_border_single_verticalRight_s char_border_single_horizontal_s char_border_single_horizontal_s
          char_border_single_horizontal_s char_border_single_horizontal_s
              char_border_single_horizontal_s char_border_single_horizontal_s
                  char_border_single_horizontal_s char_border_single_horizontal_s
                      char_border_single_horizontal_s char_border_single_horizontal_s
                          char_border_single_horizontal_s char_border_single_horizontal_s
                              char_border_single_horizontal_s char_border_single_horizontal_s
                                  char_border_single_horizontal_s char_border_single_horizontal_s
                                      char_border_single_horizontal_s char_border_single_horizontal_s
                                          char_border_single_horizontal_s
                                              char_border_single_horizontal_s
                                                  char_border_single_horizontal_s
                                                      char_border_single_horizontal_s
                                                          char_border_single_horizontal_s
                                                              char_border_single_horizontal_s
                                                                  char_border_single_verticalLeft_s,
      props);
  DrawString(
      x, y + h - 1,
      char_border_single_bottomLeft_s char_border_single_horizontal_s char_border_single_horizontal_s
          char_border_single_horizontal_s char_border_single_horizontal_s
              char_border_single_horizontal_s char_border_single_horizontal_s
                  char_border_single_horizontal_s char_border_single_horizontal_s
                      char_border_single_horizontal_s char_border_single_horizontal_s
                          char_border_single_horizontal_s char_border_single_horizontal_s
                              char_border_single_horizontal_s char_border_single_horizontal_s
                                  char_border_single_horizontal_s char_border_single_horizontal_s
                                      char_border_single_horizontal_s char_border_single_horizontal_s
                                          char_border_single_horizontal_s
                                              char_border_single_horizontal_s
                                                  char_border_single_horizontal_s
                                                      char_border_single_horizontal_s
                                                          char_border_single_horizontal_s
                                                              char_border_single_horizontal_s
                                                                  char_border_single_bottomRight_s,
      props);

  // selection
  SetColor(CD_HILITE2);
  DrawString(x + 1 + col_ * 2, y + 6 + row_ * 2,
             char_filledBorder_topLeftCorner_s char_block_bottom_s
                 char_filledBorder_topRightCorner_s,
             props);
  DrawString(x + 1 + col_ * 2, y + 7 + row_ * 2, char_block_left_s, props);
  DrawString(x + 3 + col_ * 2, y + 7 + row_ * 2, char_block_right_s, props);
  DrawString(x + 1 + col_ * 2, y + 8 + row_ * 2,
             char_filledBorder_bottomLeftCorner_s char_block_top_s
                 char_filledBorder_bottomRightCorner_s,
             props);

  // help texts
  SetColor(CD_INFO);
  DrawString(x + 1, y + 16,
             "    aA " char_button_edit_s "   " char_border_single_vertical_s
             "  <,> " char_button_alt_s "+" char_button_left_s
             "/" char_button_right_s,
             props);
  DrawString(x + 1, y + 17,
             "   use " char_button_enter_s "   " char_border_single_vertical_s
             "  del " char_button_alt_s "+" char_button_enter_s,
             props);
  DrawString(x + 1, y + 18,
             " abort " char_button_alt_s "+" char_button_nav_s
             " " char_border_single_vertical_s " save " char_button_nav_s
             "+" char_button_left_s,
             props);

  // title
  SetColor(CD_NORMAL);
  DrawString(x + 1, y + 1, title_, props);

  // content
  const int tY = y + 4;
  DrawString(x + 1, tY, char_button_border_left_s, props);
  DrawString(x + w - 2, tY, char_button_border_right_s, props);

  snprintf(buffer, sizeof(buffer), "%-22s", text_);
  props.invert_ = true;
  DrawString(x + 2, tY, buffer, props);

  // currently edited character
  SetColor(CD_HILITE1);
  buffer[1] = 0;
  buffer[0] = editColumn_ < strlen(text_) ? text_[editColumn_] : '_';
  DrawString(x + 2 + editColumn_, tY, buffer, props);
}

void TextInputViewModal::OnPlayerUpdate(PlayerEventType,
                                        unsigned int currentTick) {}

void TextInputViewModal::OnFocus() {}

void TextInputViewModal::ProcessButtonMask(unsigned short mask, bool pressed) {
  if (!pressed) {
    return;
  }

  // navigate the input field
  if (mask == EPBM_LEFT) {
    updatePosition(-1, 0);
  } else if (mask == EPBM_RIGHT) {
    updatePosition(1, 0);
  } else if (mask == EPBM_UP) {
    updatePosition(0, -1);
  } else if (mask == EPBM_DOWN) {
    updatePosition(0, 1);
  } else if (mask == EPBM_EDIT) {
    // toggle case
    upperCase_ = !upperCase_;
  } else if (mask == EPBM_ENTER) {
    // enter -> use character
    insertCharacter();
  } else if (mask & EPBM_ALT) {
    // alt + ...
    if (mask & EPBM_LEFT) {
      // left: move cursor
      updateEditPosition(-1);
    } else if (mask & EPBM_RIGHT) {
      // right: move cursor
      updateEditPosition(1);
    } else if (mask & EPBM_NAV) {
      // nav -> abort
      EndModal(MBL_CANCEL);
    } else if (mask & EPBM_ENTER) {
      // enter -> cut
      cutAtCurrenctPosition();
    }
  } else if (mask & EPBM_NAV) {
    // nav + ...
    if (mask & EPBM_LEFT) {
      // left: save and exit
      EndModal(MBL_YES);
    }
  }

  isDirty_ = true;
}

void TextInputViewModal::AnimationUpdate() {}

void TextInputViewModal::insertCharacter() {
  const char *chars = upperCase_ ? upperChars : lowerChars;

  if ((int)editColumn_ < maxTextLength_ && editColumn_ < sizeof(text_) - 1) {
    char c = chars[col_ + row_ * columns_];
    int len = strlen(text_);
    if ((int)editColumn_ <= len) {
      // Insert character at editColumn_
      memmove(&text_[editColumn_ + 1], &text_[editColumn_],
              len - editColumn_ + 1);
      text_[editColumn_] = c;
      editColumn_++;
    }
  }
}

void TextInputViewModal::updatePosition(int deltaX, int deltaY) {
  // Handle navigation around gaps in row 3 (bottom row with spaces)
  if (row_ == 3 && deltaX != 0) {
    // Moving left: skip over gaps
    if (deltaX == -1 && col_ == 4)
      deltaX = -3; // Skip cols 2-3
    else if (deltaX == -1 && col_ == 9)
      deltaX = -2; // Skip cols 7-8
    else if (deltaX == -1 && col_ == 0)
      deltaX = -2; // Wrap and skip col_ 10
    // Moving right: skip over gaps
    else if (deltaX == 1 && col_ == 1)
      deltaX = 3; // Skip cols 2-3
    else if (deltaX == 1 && (col_ == 7 || col_ == 9))
      deltaX = 2; // Skip gaps
  }

  // Handle vertical navigation: adjust column when moving to/from row_ 3 gaps
  if ((row_ == 2 && deltaY == 1) || (row_ == 0 && deltaY == -1)) {
    if (col_ == 2)
      deltaX = -1; // Shift left from gap
    else if (col_ == 3)
      deltaX = 1; // Shift right from gap
    else if (col_ == 8 || col_ == 10)
      col_ = 9; // Snap to valid column
  }

  col_ = (col_ + deltaX + columns_) % columns_;
  row_ = (row_ + deltaY + rows_) % rows_;
}

void TextInputViewModal::updateEditPosition(int delta) {
  int len = strlen(text_);
  if (delta == -1 && editColumn_ > 0) {
    editColumn_--;
  } else if (delta == 1 && (int)editColumn_ < len) {
    editColumn_++;
  }
}

void TextInputViewModal::cutAtCurrenctPosition() {
  size_t len = strlen(text_);
  if (editColumn_ < len) {
    memmove(&text_[editColumn_], &text_[editColumn_ + 1], len - editColumn_);
  } else {
    text_[editColumn_] = '\0';
  }

  if (editColumn_ > 0) {
    editColumn_--;
  }
}

char *TextInputViewModal::getText() { return text_; }