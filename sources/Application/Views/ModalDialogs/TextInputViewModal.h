
#include "Application/Views/BaseClasses/ModalView.h"
#include "Externals/etl/include/etl/string.h"

class TextInputViewModal : public ModalView {
public:
  TextInputViewModal(View &view, const char *title, const char *value,
                     int maxTextLength);

  static TextInputViewModal *Create(View &view, const char *title,
                                    const char *value, int maxTextLength);

  virtual void Destroy() override;

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void AnimationUpdate();

  char *getText();

private:
  const char *lowerChars = "abcdefgh123ijklmnop456qrstuvwx789yz  ,._- 0 ";
  const char *upperChars = "ABCDEFGH123IJKLMNOP456QRSTUVWX789YZ  ,._- 0 ";

  static bool inUse_;
  static void *storage_;

  const char *title_;
  const int maxTextLength_;

  char text_[32]; // Buffer to hold the current text input value
  unsigned int editColumn_;

  const int columns_ = 11;
  const int rows_ = 4;

  bool upperCase_;
  int row_ = 0;
  int col_ = 0;

  void updatePosition(int deltaX, int deltaY);
  void updateEditPosition(int delta);
  void insertCharacter();
  void cutAtCurrenctPosition();
};