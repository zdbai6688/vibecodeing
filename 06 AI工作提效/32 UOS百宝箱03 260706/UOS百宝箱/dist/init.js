// UOS百宝箱 init.js
// 说明: TOOLS 和 SYSTEM_TOGGLES 已在 index.html 内联脚本中定义,
// 此文件仅保留页面初始化逻辑, 避免变量重复声明导致白屏。
document.addEventListener('DOMContentLoaded', function() {
  if (typeof renderNav === 'function') {
    renderNav();
    if (typeof NAV !== 'undefined' && NAV[0]) {
      switchPage(NAV[0].id);
    }
  }
});
