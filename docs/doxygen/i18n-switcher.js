(function () {
  function currentFileName() {
    var parts = window.location.pathname.split("/");
    var last = parts[parts.length - 1];
    return last || "index.html";
  }

  function currentLanguage() {
    var parts = window.location.pathname.split("/");
    if (parts.indexOf("ja") >= 0) {
      return "ja";
    }

    return "en";
  }

  function buildTarget(fileName, hash) {
    if (!fileName) {
      return null;
    }

    return fileName + hash;
  }

  function buildOption(label, target, isActive) {
    var node = document.createElement(isActive || !target ? "span" : "a");
    node.className = "docs-language-switcher-option";
    node.textContent = label;

    if (isActive) {
      node.className += " is-active";
      node.setAttribute("aria-current", "true");
      return node;
    }

    if (!target) {
      node.className += " is-disabled";
      node.title = "This page does not have a translated counterpart.";
      node.setAttribute("aria-disabled", "true");
      return node;
    }

    node.href = target;
    return node;
  }

  function installSwitcher() {
    var mount = document.getElementById("projectalign");
    if (!mount || document.getElementById("docs-language-switcher")) {
      return;
    }

    var map = window.DOCS_I18N_MAP || {};
    var fileName = currentFileName();
    var lang = currentLanguage();
    var alternate = map[fileName] || null;
    var hash = window.location.hash || "";
    var enTarget = lang === "en" ? fileName : alternate;
    var jaTarget = lang === "ja" ? fileName : alternate;

    var wrapper = document.createElement("div");
    wrapper.id = "docs-language-switcher";

    var label = document.createElement("span");
    label.id = "docs-language-switcher-label";
    label.textContent = "Language";

    var buttons = document.createElement("div");
    buttons.id = "docs-language-switcher-buttons";
    buttons.appendChild(buildOption("EN", buildTarget(enTarget, hash), lang === "en"));
    buttons.appendChild(buildOption("JA", buildTarget(jaTarget, hash), lang === "ja"));

    wrapper.appendChild(label);
    wrapper.appendChild(buttons);
    mount.appendChild(wrapper);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", installSwitcher);
  } else {
    installSwitcher();
  }
})();
