// Offset: 0x0ADCF1

  (function (doc, win) { var docEl = doc.documentElement, resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize", recalc = function () { var clientWidth = docEl.clientWidth; if (!clientWidth) { return } if (clientWidth >= 750) { docEl.style.fontSize = "100px" } else { docEl.style.fontSize = 100 * (clientWidth / 750) + "px" } }; if (!doc.addEventListener) { return } win.addEventListener(resizeEvt, recalc, false); doc.addEventListener("DOMContentLoaded", recalc, false) })(document, window); function $(id) { return document.getElementById(id) } function $$(id) { return document.getElementsByName(id) } function AJAX(url, callback) { var req = AJAX_init(); req.onreadystatechange = AJAX_processRequest; function AJAX_init() { if (window.XMLHttpRequest) { return new XMLHttpRequest() } else { if (window.ActiveXObject) { return new ActiveXObject("Microsoft.XMLHTTP") } } } function AJAX_processRequest() { if (req.readyState == 4) { if (req.status == 200) { if (callback) { callback(req.responseText) } } } } this.doGet = function () { req.open("GET", url, true); req.send(null) }; this.doPost = function (body) { req.open("POST", url, true); req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded"); req.setRequestHeader("ISAJAX", "yes"); req.send(body) } } (function () { var navWrap = document.getElementById("side_nav"); var nav1s = navWrap.getElementsByTagName("li"); var nav2s = navWrap.getElementsByTagName("ul"); for (var i = 0, len = nav1s.length; i < len; i++) { nav1s[i].onclick = (function (i) { return function () { if (nav2s[i].style.display == "none") { nav2s[i].style.display = "block" } else { nav2s[i].style.display = "none" } } })(i) } })(); function logout(o) { var win = confirm("Are you sure you want to logout?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "logout.cgi"; cgiform.submit() } else { return false } } function reboot(o) { var win = confirm("Are you sure you want to reboot?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "reboot_btn.cgi"; cgiform.submit() } else { return false } } function tabChange(o) { var url = o.getAttribute("pageid") + ".cgi"; cgiform.action = url; cgiform.submit() } function UpdatePageCallback(req) { var pageId = req.pageId; var emList = document.getElementsByTagName("strong"); for (var index = 0; index < emList.length; index++) { if (emList[index].getAttribute("pageid") == pageId) { emList[index].setAttribute("class", "cur"); emList[index].previousElementSibling.setAttribute("class", "iconfont") } else { emList[index].setAttribute("class", ""); emList[index].previousElementSibling.setAttribute("class", "") } } } function formatDateTime(inputTime) { var date = new Date(inputTime); var y = date.getFullYear(); var m = date.getMonth() + 1; m = m < 10 ? ("0" + m) : m; var d = date.getDate(); d = d < 10 ? ("0" + d) : d; var h = date.getHours(); h = h < 10 ? ("0" + h) : h; var minute = date.getMinutes(); var second = date.getSeconds(); minute = minute < 10 ? ("0" + minute) : minute; second = second < 10 ? ("0" + second) : second; return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second } function minerinfoCallback(req) { req.hwtype !== undefined ? req.hwtype : ""; req.mac !== undefined ? req.mac : ""; req.ipv4 !== undefined ? req.ipv4 : ""; req.version !== undefined ? req.version : ""; $("hwtype").innerHTML = req.hwtype; $("loadtime").innerHTML = formatDateTime(new Date().getTime()); var html = ""; var css = ""; if (req.sys_status == "1") { html = "Online"; css = "status on" } else { html = "Idle"; css = "status off" } $("sys_status").innerHTML = html; $("sys_status").setAttribute("class", css); $("mac").innerHTML = req.mac; $("ipv4").innerHTML = req.ipv4; $("version").innerHTML = req.version } function updateMinerInfo() { var oUpdate; oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet(); setTimeout(updateMinerInfo, 15000) } updateMinerInfo(); function updateLog() { var oUpdate; oUpdate = new AJAX("updatecglog.cgi?num=" + Math.random(), function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet(); setTimeout(updateLog, 5000) } function DebugSwitch(o) { var p = o.name; var oDebug = new AJAX("cglog.cgi", function (t) { try { eval(t) } catch (e) { } }); oDebug.doPost("debugswitchbtn=" + p) } function CgLogCallback(req) { console.log("log==" + req.cglog); $("cglog").innerHTML = req.cglog } updateLog();


// Offset: 0x0B50B4

  (function (doc, win) {
    var docEl = doc.documentElement,
      resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize",
      recalc = function () {
        var clientWidth = docEl.clientWidth;
        if (!clientWidth) {
          return
        }
        if (clientWidth >= 750) {
          docEl.style.fontSize = "100px"
        } else {
          docEl.style.fontSize = 100 * (clientWidth / 750) + "px"
        }
      };
    if (!doc.addEventListener) {
      return
    }
    win.addEventListener(resizeEvt, recalc, false);
    doc.addEventListener("DOMContentLoaded", recalc, false)
  })(document, window);
  function $(id) {
    return document.getElementById(id)
  }
  function $$(id) {
    return document.getElementsByName(id)
  }
  function AJAX(url, callback) {
    var req = AJAX_init();
    req.onreadystatechange = AJAX_processRequest;
    function AJAX_init() {
      if (window.XMLHttpRequest) {
        return new XMLHttpRequest()
      } else {
        if (window.ActiveXObject) {
          return new ActiveXObject("Microsoft.XMLHTTP")
        }
      }
    }
    function AJAX_processRequest() {
      if (req.readyState == 4) {
        if (req.status == 200) {
          if (callback) {
            callback(req.responseText)
          }
        }
      }
    }
    this.doGet = function () {
      req.open("GET", url, true);
      req.send(null)
    };
    this.doPost = function (body) {
      req.open("POST", url, true);
      req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      req.setRequestHeader("ISAJAX", "yes");
      req.send(body)
    }
  } (function () {
    var navWrap = document.getElementById("side_nav");
    var nav1s = navWrap.getElementsByTagName("li");
    var nav2s = navWrap.getElementsByTagName("ul");
    for (var i = 0,
      len = nav1s.length; i < len; i++) {
      nav1s[i].onclick = (function (i) {
        return function () {
          if (nav2s[i].style.display == "none") {
            nav2s[i].style.display = "block"
          } else {
            nav2s[i].style.display = "none"
          }
        }
      })(i)
    }
  })();
  function logout(o) {
    var win = confirm("Are you sure you want to logout?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "logout.cgi";
      cgiform.submit()
    } else {
      return false
    }
  }
  function reboot(o) {
    var win = confirm("Are you sure you want to reboot?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "reboot_btn.cgi";
      cgiform.submit()
    } else {
      return false
    }
  }
  function tabChange(o) {
    var url = o.getAttribute("pageid") + ".cgi";
    cgiform.action = url;
    cgiform.submit()
  }
  function UpdatePageCallback(req) {
    var pageId = req.pageId;
    var emList = document.getElementsByTagName("strong");
    for (var index = 0; index < emList.length; index++) {
      if (emList[index].getAttribute("pageid") == pageId) {
        emList[index].setAttribute("class", "cur");
        emList[index].previousElementSibling.setAttribute("class", "iconfont")
      } else {
        emList[index].setAttribute("class", "");
        emList[index].previousElementSibling.setAttribute("class", "")
      }
    }
  }
  function formatDateTime(inputTime) {
    var date = new Date(inputTime);
    var y = date.getFullYear();
    var m = date.getMonth() + 1;
    m = m < 10 ? ("0" + m) : m;
    var d = date.getDate();
    d = d < 10 ? ("0" + d) : d;
    var h = date.getHours();
    h = h < 10 ? ("0" + h) : h;
    var minute = date.getMinutes();
    var second = date.getSeconds();
    minute = minute < 10 ? ("0" + minute) : minute;
    second = second < 10 ? ("0" + second) : second;
    return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second
  }
  function minerinfoCallback(req) {
    req.hwtype !== undefined ? req.hwtype : "";
    req.mac !== undefined ? req.mac : "";
    req.ipv4 !== undefined ? req.ipv4 : "";
    req.version !== undefined ? req.version : "";
    $("hwtype").innerHTML = req.hwtype;
    $("loadtime").innerHTML = formatDateTime(new Date().getTime());
    var html = "";
    var css = "";
    if (req.sys_status == "1") {
      html = "Online";
      css = "status on"
    } else {
      html = "Idle";
      css = "status off"
    }
    $("sys_status").innerHTML = html;
    $("sys_status").setAttribute("class", css);
    $("mac").innerHTML = req.mac;
    $("ipv4").innerHTML = req.ipv4;
    $("version").innerHTML = req.version
  }
  function updateMinerInfo() {
    var oUpdate;
    oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(),
      function (t) {
        try {
          eval(t)
        } catch (e) { }
      });
    oUpdate.doGet();
    setTimeout(updateMinerInfo, 15000)
  }
  updateMinerInfo();
  function updateCGConf() {
    var oUpdate;
    oUpdate = new AJAX("updatecgconf.cgi",
      function (t) {
        try {
          eval(t)
        } catch (e) { }
      });
    oUpdate.doGet()
  }
  function selset(id, val) {
    var o = $(id);
    for (var i = 0; i < o.options.length; i++) {
      if (i == val) {
        o.options[i].selected = true;
        break
      }
    }
  }
  function CGConfCallback(req) {
    $("worker1").value = req.worker1;
    $("passwd1").value = req.passwd1;
    $("worker2").value = req.worker2;
    $("passwd2").value = req.passwd2;
    $("worker3").value = req.worker3;
    $("passwd3").value = req.passwd3;
    $("pool1").value = req.pool1;
    $("pool2").value = req.pool2;
    $("pool3").value = req.pool3;
    selset("mode", req.mode)
  }
  function confChk(o) {
    var len = o.value.length;
    var pattern = /(^[0-9.]+$)/;
    if (!pattern.test(o.value)) {
      alert("Allow only numbers and dot(.)");
      o.value = o.value.substring(0, (o.value.length - 1))
    }
  }
  function submitcgminercfg(o) {
    cgconfform.submit();
    alert("Save successfully, Need reboot manualy")
  }
  updateCGConf();


// Offset: 0x0BBBC8

    (function (doc, win) { var docEl = doc.documentElement, resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize", recalc = function () { var clientWidth = docEl.clientWidth; if (!clientWidth) { return } if (clientWidth >= 750) { docEl.style.fontSize = "100px" } else { docEl.style.fontSize = 100 * (clientWidth / 750) + "px" } }; if (!doc.addEventListener) { return } win.addEventListener(resizeEvt, recalc, false); doc.addEventListener("DOMContentLoaded", recalc, false) })(document, window); function $(id) { return document.getElementById(id) } function $$(id) { return document.getElementsByName(id) } function AJAX(url, callback) { var req = AJAX_init(); req.onreadystatechange = AJAX_processRequest; function AJAX_init() { if (window.XMLHttpRequest) { return new XMLHttpRequest() } else { if (window.ActiveXObject) { return new ActiveXObject("Microsoft.XMLHTTP") } } } function AJAX_processRequest() { if (req.readyState == 4) { if (req.status == 200) { if (callback) { callback(req.responseText) } } } } this.doGet = function () { req.open("GET", url, true); req.send(null) }; this.doPost = function (body) { req.open("POST", url, true); req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded"); req.setRequestHeader("ISAJAX", "yes"); req.send(body) } } (function () { var navWrap = document.getElementById("side_nav"); var nav1s = navWrap.getElementsByTagName("li"); var nav2s = navWrap.getElementsByTagName("ul"); for (var i = 0, len = nav1s.length; i < len; i++) { nav1s[i].onclick = (function (i) { return function () { if (nav2s[i].style.display == "none") { nav2s[i].style.display = "block" } else { nav2s[i].style.display = "none" } } })(i) } })(); function logout(o) { var win = confirm("Are you sure you want to logout?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "logout.cgi"; cgiform.submit() } else { return false } } function reboot(o) { var win = confirm("Are you sure you want to reboot?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "reboot_btn.cgi"; cgiform.submit() } else { return false } } function tabChange(o) { var url = o.getAttribute("pageid") + ".cgi"; cgiform.action = url; cgiform.submit() } function UpdatePageCallback(req) { var pageId = req.pageId; var emList = document.getElementsByTagName("strong"); for (var index = 0; index < emList.length; index++) { if (emList[index].getAttribute("pageid") == pageId) { emList[index].setAttribute("class", "cur"); emList[index].previousElementSibling.setAttribute("class", "iconfont") } else { emList[index].setAttribute("class", ""); emList[index].previousElementSibling.setAttribute("class", "") } } } function formatDateTime(inputTime) { var date = new Date(inputTime); var y = date.getFullYear(); var m = date.getMonth() + 1; m = m < 10 ? ("0" + m) : m; var d = date.getDate(); d = d < 10 ? ("0" + d) : d; var h = date.getHours(); h = h < 10 ? ("0" + h) : h; var minute = date.getMinutes(); var second = date.getSeconds(); minute = minute < 10 ? ("0" + minute) : minute; second = second < 10 ? ("0" + second) : second; return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second } function minerinfoCallback(req) { req.hwtype !== undefined ? req.hwtype : ""; req.mac !== undefined ? req.mac : ""; req.ipv4 !== undefined ? req.ipv4 : ""; req.version !== undefined ? req.version : ""; $("hwtype").innerHTML = req.hwtype; $("loadtime").innerHTML = formatDateTime(new Date().getTime()); var html = ""; var css = ""; if (req.sys_status == "1") { html = "Online"; css = "status on" } else { html = "Idle"; css = "status off" } $("sys_status").innerHTML = html; $("sys_status").setAttribute("class", css); $("mac").innerHTML = req.mac; $("ipv4").innerHTML = req.ipv4; $("version").innerHTML = req.version } function updateMinerInfo() { var oUpdate; oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet(); setTimeout(updateMinerInfo, 15000) } updateMinerInfo(); function changePasswd() { if (adminform.passwd.value == "") { alert("New Password is not null!"); return false } if (adminform.confirm.value == "") { alert("Confirm is not null!"); return false } if (adminform.passwd.value != adminform.confirm.value) { alert("Password and confirm password disagree"); return false } adminform.submit() } function updateadmin() { var oUpdate; oUpdate = new AJAX("updateadmin.cgi", function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet() } updateadmin();


// Offset: 0x0C033B

    function checkLogin(){ 
    	if (loginform.username.value == "") { 
    		alert("Username is null!");
    		return false;
    	}
    	if(loginform.passwd.value == ""){ 
			alert("Password is null!");
    		return false;
    	}
    	loginform.submit();
    }      
    

// Offset: 0x0C0716

            var i = 20;
            var intervalid;
            intervalid = setInterval("fun()", 1000);

            function fun() {
                if (i == 0) {
                    window.location.href = "/";
                    clearInterval(intervalid)
                }
                document.getElementById("mes").innerHTML = i;
                i--
            }
        

// Offset: 0x0C097C

            var xhr = new XMLHttpRequest();
            xhr.open('POST', 'reboot.cgi');
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.onload = function () { };
            xhr.send(encodeURI('reboot=1'));
        

// Offset: 0x0C6DF0

  (function (doc, win) {
    var docEl = doc.documentElement,
      resizeEvt =
        "onorientationchange" in window ? "onorientationchange" : "resize",
      recalc = function () {
        var clientWidth = docEl.clientWidth;
        if (!clientWidth) {
          return;
        }
        if (clientWidth >= 750) {
          docEl.style.fontSize = "100px";
        } else {
          docEl.style.fontSize = 100 * (clientWidth / 750) + "px";
        }
      };
    if (!doc.addEventListener) {
      return;
    }
    win.addEventListener(resizeEvt, recalc, false);
    doc.addEventListener("DOMContentLoaded", recalc, false);
  })(document, window);
  function $(id) {
    return document.getElementById(id);
  }
  function $$(id) {
    return document.getElementsByName(id);
  }
  function AJAX(url, callback) {
    var req = AJAX_init();
    req.onreadystatechange = AJAX_processRequest;
    function AJAX_init() {
      if (window.XMLHttpRequest) {
        return new XMLHttpRequest();
      } else {
        if (window.ActiveXObject) {
          return new ActiveXObject("Microsoft.XMLHTTP");
        }
      }
    }
    function AJAX_processRequest() {
      if (req.readyState == 4) {
        if (req.status == 200) {
          if (callback) {
            callback(req.responseText);
          }
        }
      }
    }
    this.doGet = function () {
      req.open("GET", url, true);
      req.send(null);
    };
    this.doPost = function (body) {
      req.open("POST", url, true);
      req.setRequestHeader(
        "Content-Type",
        "application/x-www-form-urlencoded"
      );
      req.setRequestHeader("ISAJAX", "yes");
      req.send(body);
    };
  }
  (function () {
    var navWrap = document.getElementById("side_nav");
    var nav1s = navWrap.getElementsByTagName("li");
    var nav2s = navWrap.getElementsByTagName("ul");
    for (var i = 0, len = nav1s.length; i < len; i++) {
      nav1s[i].onclick = (function (i) {
        return function () {
          if (nav2s[i].style.display == "none") {
            nav2s[i].style.display = "block";
          } else {
            nav2s[i].style.display = "none";
          }
        };
      })(i);
    }
  })();
  function logout(o) {
    var win = confirm("Are you sure you want to logout?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "logout.cgi";
      cgiform.submit();
    } else {
      return false;
    }
  }
  function reboot(o) {
    var win = confirm("Are you sure you want to reboot?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "reboot_btn.cgi";
      cgiform.submit();
    } else {
      return false;
    }
  }
  function tabChange(o) {
    var url = o.getAttribute("pageid") + ".cgi";
    cgiform.action = url;
    cgiform.submit();
  }
  function UpdatePageCallback(req) {
    var pageId = req.pageId;
    var emList = document.getElementsByTagName("strong");
    for (var index = 0; index < emList.length; index++) {
      if (emList[index].getAttribute("pageid") == pageId) {
        emList[index].setAttribute("class", "cur");
        emList[index].previousElementSibling.setAttribute(
          "class",
          "iconfont"
        );
      } else {
        emList[index].setAttribute("class", "");
        emList[index].previousElementSibling.setAttribute("class", "");
      }
    }
  }
  function formatDateTime(inputTime) {
    var date = new Date(inputTime);
    var y = date.getFullYear();
    var m = date.getMonth() + 1;
    m = m < 10 ? "0" + m : m;
    var d = date.getDate();
    d = d < 10 ? "0" + d : d;
    var h = date.getHours();
    h = h < 10 ? "0" + h : h;
    var minute = date.getMinutes();
    var second = date.getSeconds();
    minute = minute < 10 ? "0" + minute : minute;
    second = second < 10 ? "0" + second : second;
    return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second;
  }
  function minerinfoCallback(req) {
    req.hwtype !== undefined ? req.hwtype : "";
    req.mac !== undefined ? req.mac : "";
    req.ipv4 !== undefined ? req.ipv4 : "";
    req.version !== undefined ? req.version : "";
    req.stm8 !== undefined ? req.stm8 : "";
    $("hwtype").innerHTML = req.hwtype;
    $("loadtime").innerHTML = formatDateTime(new Date().getTime());
    var html = "";
    var css = "";
    if (req.sys_status == "1") {
      html = "Online";
      css = "status on";
    } else {
      html = "Idle";
      css = "status off";
    }
    $("sys_status").innerHTML = html;
    $("sys_status").setAttribute("class", css);
    $("mac").innerHTML = req.mac;
    $("ipv4").innerHTML = req.ipv4;
    $("version").innerHTML = req.version;
    stm8 = req.stm8;
  }

  function updateMinerInfo() {
    var oUpdate;
    oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (
      t
    ) {
      try {
        eval(t);
      } catch (e) { }
    });
    oUpdate.doGet();
    setTimeout(updateMinerInfo, 15000);
  }
  updateMinerInfo();
  function decToHex(dec, bytes) {
    hex = dec.toString(16);
    while (hex.length < bytes) {
      hex = "0" + hex;
    }
    return hex;
  }
  function hex_to_ascii(str) {
    var arr = [];
    for (var i = 0, l = str.length; i < l; i++) {
      var hex = Number(str.charCodeAt(i)).toString(16);
      if (hex.length < 2) {
        hex = "0" + hex;
      }
      arr.push(hex);
    }
    return arr.join("");
  }
  var pkg_num = 0;
  var offset = 0;
  var reply;
  var files;
  var timer;
  var begin_timer;
  var repeat_cnt = 0;
  var end = 0;
  var flash_time = 120;
  var intervalid;
  var _width = 0;
  var progress_timer;
  var stm8;
  FileReader.prototype.readAsBinaryString = function (fileData) {
    var binary = "";
    var pt = this;
    var reader = new FileReader();
    reader.onload = function (e) {
      var bytes = new Uint8Array(reader.result);
      var length = bytes.byteLength;
      for (var i = 0; i < length; i++) {
        binary += String.fromCharCode(bytes[i]);
      }
      pt.content = binary;
      pt.onload(pt);
    };
    reader.readAsArrayBuffer(fileData);
  };
  function load_data() {
    var cgi_reply = document.getElementById("cont").innerHTML;
    if (cgi_reply == "OK" + decToHex(pkg_num, 4)) {
      pkg_num = pkg_num + 1;
      offset = offset + 256;
      repeat_cnt = 0;
      document.getElementById("cont").innerHTML = "0";
    }
    var reader = new FileReader();
    end = offset + 256 < files[0].size ? offset + 256 : files[0].size;
    reader.readAsBinaryString(files[0].slice(offset, end));
    reader.onload = function (file) {
      var buf_index = 0;
      var pkg_len = file.content.length;
      var oPost = new AJAX("upload.cgi?num=" + Math.random(), function (t) {
        try {
        } catch (e) {
          alert(e);
        }
        reply = t;
        document.getElementById("cont").innerHTML = reply;
      });
      oPost.doPost(
        "uploadbtn=" +
        decToHex(parseInt(pkg_num >> 8), 2) +
        decToHex(parseInt(pkg_num & 255), 2) +
        decToHex(parseInt(pkg_len >> 8), 2) +
        decToHex(parseInt(pkg_len & 255), 2) +
        decToHex(repeat_cnt & 255, 2) +
        hex_to_ascii(file.content)
      );
    };
    timer = setTimeout(load_data, 60);
    if (offset >= files[0].size) {
      clearTimeout(timer);
      end_cmd();
    }
    repeat_cnt = repeat_cnt + 1;
    if (repeat_cnt > 512) {
      repeat_cnt = 0;
      clearTimeout(timer);
      alert("send  img packet timeout");
    }
  }
  function begin_cmd(img_len) {
    var oPost = new AJAX("upload.cgi?num=" + Math.random(), function (t) {
      try {
      } catch (e) {
        alert(e);
      }
      document.getElementById("cont").innerHTML = t;
    });
    img_len = files[0].size;
    oPost.doPost(
      "begin=" +
      decToHex(parseInt(img_len >> 24) & 255, 2) +
      decToHex(parseInt(img_len >> 16) & 255, 2) +
      decToHex(parseInt(img_len >> 8) & 255, 2) +
      decToHex(parseInt(img_len & 255), 2)
    );
    begin_timer = setTimeout(begin_cmd, 100);
    if (document.getElementById("cont").innerHTML == "OK") {
      clearTimeout(begin_timer);
      document.getElementById("cont").innerHTML = "0";
      load_data();
    }
    if (document.getElementById("cont").innerHTML == "ERR-0") {
      document.getElementById("cont").innerHTML = "0";
      clearTimeout(begin_timer);
      alert("image file too bigger");
    }
  }
  function end_cmd() {
    var oPost = new AJAX("upload.cgi?num=" + Math.random(), function (t) {
      try {
      } catch (e) {
        alert(e);
      }
      document.getElementById("cont").innerHTML = t;
    });
    oPost.doPost("end=");
  }
  function upgrade_confim(rep) {
    var win = confirm(
      "Image File Information:\r\n" +
      "name:" +
      rep.filename +
      "\r\nsize:" +
      rep.size
    );
    if (win == true) {
      load_progress();
      begin_cmd(rep.size);
      showAndHidden(0);
    } else {
      return false;
    }
  }
  function UploadFirmware(o) {
    var p = o.name;
    var img_info;
    var files_source = document.getElementById("firmware");
    files = files_source.files;
    pkg_num = 0;
    offset = 0;
    document.getElementById("cont").innerHTML = "0";
    img_info = { filename: files[0].name, size: files[0].size };
    //var stm8 = document.getElementById("stm8").value;
    if(typeof(stm8)!="undefined"&&stm8!=null&&stm8!="") {
      if(img_info.filename.toLowerCase().indexOf("plus") == -1){
        alert("this device contains property stm8, upgrade file must be plus version!");
        return;
      }
    } else {
      if(img_info.filename.toLowerCase().indexOf("plus") != -1){
        alert("this device don't contains property stm8, upgrade file must not be plus version!");
        return;
      }
    }
    upgrade_confim(img_info);
  }
  function updateupgrade() {
    var oUpdate;
    oUpdate = new AJAX("updateupgrade.cgi", function (t) {
      try {
        eval(t);
      } catch (e) { }
    });
    oUpdate.doGet();
  }
  function get_upgrade_result() {
    var oUpdate;
    oUpdate = new AJAX("upgrade_status.cgi?num=" + Math.random(), function (
      t
    ) {
      try {
        eval(t);
      } catch (e) { }
      document.getElementById("result").innerHTML = t;
    });
    oUpdate.doGet();
  }
  function load_progress() {
    progress_timer = setTimeout(load_progress, 200);
    _width = (offset / files[0].size) * 490;
    if (_width >= 490) {
      clearTimeout(progress_timer);
      showAndHidden(1);
    }
    document.getElementsByClassName("inner")[0].style.width = _width + "px";
    var _data = Math.floor((_width / 490) * 100);
    document.getElementsByClassName("data")[0].innerText = _data + "%";
    document.getElementsByClassName("data")[0].style.left =
      256 + _width + "px";
  }
  function burn_progress() {
    get_upgrade_result();
    intervalid = setTimeout(burn_progress, 1000);
    if (
      document.getElementById("result").innerHTML == 7 ||
      document.getElementById("result").innerHTML == 8 ||
      document.getElementById("result").innerHTML == "OK"
    ) {
      clearTimeout(intervalid);
      if (document.getElementById("result").innerHTML == "OK") {
        clearTimeout(intervalid);
        document.getElementById("mes").innerHTML =
          "Upgrade successully ,please reboot device manully";
      } else {
        alert("Upgrade faild");
        document.getElementById("mes").innerHTML = "Upgrade faild";
      }
    } else {
      if (flash_time == 0) {
        clearTimeout(intervalid);
        document.getElementById("mes").innerHTML = "Program flash timeout";
      } else {
        document.getElementById("mes").innerHTML =
          "Program flash..., will be finished after (" +
          flash_time +
          ") second";
        flash_time--;
      }
    }
  }
  function showAndHidden(divflag) {
    var div1 = document.getElementById("div1");
    var div2 = document.getElementById("div2");
    var div3 = document.getElementById("div3");
    if (divflag == 1) {
      div1.style.display = "none";
      div2.style.display = "none";
      div3.style.display = "block";
      flash_time = 120;
      burn_progress();
    } else {
      if (div1.style.display == "block") {
        div1.style.display = "none";
      } else {
        div1.style.display = "block";
      }
      if (div2.style.display == "block") {
        div2.style.display = "none";
      } else {
        div2.style.display = "block";
      }
      div3.style.display = "none";
    }
  }
  updateupgrade();


// Offset: 0x0CEEE1
(function(doc,win){var docEl=doc.documentElement,resizeEvt="onorientationchange" in window?"onorientationchange":"resize",recalc=function(){var clientWidth=docEl.clientWidth;if(!clientWidth){return}if(clientWidth>=750){docEl.style.fontSize="100px"}else{docEl.style.fontSize=100*(clientWidth/750)+"px"}};if(!doc.addEventListener){return}win.addEventListener(resizeEvt,recalc,false);doc.addEventListener("DOMContentLoaded",recalc,false)})(document,window);function $(id){return document.getElementById(id)}function $$(id){return document.getElementsByName(id)}function AJAX(url,callback){var req=AJAX_init();req.onreadystatechange=AJAX_processRequest;function AJAX_init(){if(window.XMLHttpRequest){return new XMLHttpRequest()}else{if(window.ActiveXObject){return new ActiveXObject("Microsoft.XMLHTTP")}}}function AJAX_processRequest(){if(req.readyState==4){if(req.status==200){if(callback){callback(req.responseText)}}}}this.doGet=function(){req.open("GET",url,true);req.send(null)};this.doPost=function(body){req.open("POST",url,true);req.setRequestHeader("Content-Type","application/x-www-form-urlencoded");req.setRequestHeader("ISAJAX","yes");req.send(body)}}(function(){var navWrap=document.getElementById("side_nav");var nav1s=navWrap.getElementsByTagName("li");var nav2s=navWrap.getElementsByTagName("ul");for(var i=0,len=nav1s.length;i<len;i++){nav1s[i].onclick=(function(i){return function(){if(nav2s[i].style.display=="none"){nav2s[i].style.display="block"}else{nav2s[i].style.display="none"}}})(i)}})();function logout(o){var win=confirm("Are you sure you want to logout?");var p=o.getAttribute("id");if(win==true){cgiform.action="logout.cgi";cgiform.submit()}else{return false}}function reboot(o){var win=confirm("Are you sure you want to reboot?");var p=o.getAttribute("id");if(win==true){cgiform.action="reboot_btn.cgi";cgiform.submit()}else{return false}}function tabChange(o){var url=o.getAttribute("pageid")+".cgi";cgiform.action=url;cgiform.submit()}function UpdatePageCallback(req){var pageId=req.pageId;var emList=document.getElementsByTagName("strong");for(var index=0;index<emList.length;index++){if(emList[index].getAttribute("pageid")==pageId){emList[index].setAttribute("class","cur");emList[index].previousElementSibling.setAttribute("class","iconfont")}else{emList[index].setAttribute("class","");emList[index].previousElementSibling.setAttribute("class","")}}}function formatDateTime(inputTime){var date=new Date(inputTime);var y=date.getFullYear();var m=date.getMonth()+1;m=m<10?("0"+m):m;var d=date.getDate();d=d<10?("0"+d):d;var h=date.getHours();h=h<10?("0"+h):h;var minute=date.getMinutes();var second=date.getSeconds();minute=minute<10?("0"+minute):minute;second=second<10?("0"+second):second;return y+"-"+m+"-"+d+" "+h+":"+minute+":"+second}function minerinfoCallback(req){req.hwtype!==undefined?req.hwtype:"";req.mac!==undefined?req.mac:"";req.ipv4!==undefined?req.ipv4:"";req.version!==undefined?req.version:"";$("hwtype").innerHTML=req.hwtype;$("loadtime").innerHTML=formatDateTime(new Date().getTime());var html="";var css="";if(req.sys_status=="1"){html="Online";css="status on"}else{html="Idle";css="status off"}$("sys_status").innerHTML=html;$("sys_status").setAttribute("class",css);$("mac").innerHTML=req.mac;$("ipv4").innerHTML=req.ipv4;$("version").innerHTML=req.version}function updateMinerInfo(){var oUpdate;oUpdate=new AJAX("get_minerinfo.cgi?num="+Math.random(),function(t){try{eval(t)}catch(e){}});oUpdate.doGet();setTimeout(updateMinerInfo,15000)}updateMinerInfo();function updateNetConf(){var oUpdate;oUpdate=new AJAX("updatenetwork.cgi",function(t){try{eval(t)}catch(e){}});oUpdate.doGet()}function netinfo_block(req){if(req==0){$("ip").disabled=true;$("mask").disabled=true;$("gateway").disabled=true;$("dns").disabled=true;$("dnsbak").disabled=true}else{$("ip").disabled=false;$("mask").disabled=false;$("gateway").disabled=false;$("dns").disabled=false;$("dnsbak").disabled=false}}function NetworkCallback(req){var obj=document.getElementsByName("protocol");$("ip").value=req.ip;$("mask").value=req.mask;$("gateway").value=req.gateway;$("dns").value=req.dns;$("dnsbak").value=req.dnsbak;for(var i=0;i<obj.length;i++){if(i==req.protocal){obj[i].checked=true;break}}netinfo_block(i)}function confChk(o){var len=o.value.length;var pattern=/(^[0-9.]+$)/;if(!pattern.test(o.value)){alert("Allow only numbers and dot(.)");o.value=o.value.substring(0,(o.value.length-1))}if(len>15){o.value=o.value.substring(0,15)}}function submitNetWork(o){networkform.submit();alert("Save successfully, Need reboot manualy")}updateNetConf();

// Offset: 0x0D8427
(function (doc, win) {
        var docEl = doc.documentElement,
            resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize",
            recalc = function () {
                var clientWidth = docEl.clientWidth;
                if (!clientWidth) {
                    return
                }
                if (clientWidth >= 750) {
                    docEl.style.fontSize = "100px"
                } else {
                    docEl.style.fontSize = 100 * (clientWidth / 750) + "px"
                }
            };
        if (!doc.addEventListener) {
            return
        }
        win.addEventListener(resizeEvt, recalc, false);
        doc.addEventListener("DOMContentLoaded", recalc, false)
    })(document, window);
    function $(id) {
        return document.getElementById(id)
    }
    function $$(id) {
        return document.getElementsByName(id)
    }
    function addClass(id, cls) {
        var elem = document.getElementById(id);
        elem.classList.add(cls)
    }
    function removeClass(id, cls) {
        var elem = document.getElementById(id);
        elem.classList.remove(cls)
    }
    function AJAX(url, callback) {
        var req = AJAX_init();
        req.onreadystatechange = AJAX_processRequest;
        function AJAX_init() {
            if (window.XMLHttpRequest) {
                return new XMLHttpRequest()
            } else {
                if (window.ActiveXObject) {
                    return new ActiveXObject("Microsoft.XMLHTTP")
                }
            }
        }
        function AJAX_processRequest() {
            if (req.readyState == 4) {
                if (req.status == 200) {
                    if (callback) {
                        callback(req.responseText)
                    }
                }
            }
        }
        this.doGet = function () {
            req.open("GET", url, true);
            req.send(null)
        };
        this.doPost = function (body) {
            req.open("POST", url, true);
            req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            req.setRequestHeader("ISAJAX", "yes");
            req.send(body)
        }
    } (function () {
        var navWrap = document.getElementById("side_nav");
        var nav1s = navWrap.getElementsByTagName("li");
        var nav2s = navWrap.getElementsByTagName("ul");
        for (var i = 0,
            len = nav1s.length; i < len; i++) {
            nav1s[i].onclick = (function (i) {
                return function () {
                    if (nav2s[i].style.display == "none") {
                        nav2s[i].style.display = "block"
                    } else {
                        nav2s[i].style.display = "none"
                    }
                }
            })(i)
        }
    })();
    function logout(o) {
        var win = confirm("Are you sure you want to logout?");
        var p = o.getAttribute("id");
        if (win == true) {
            cgiform.action = "logout.cgi";
            cgiform.submit()
        } else {
            return false
        }
    }
    function reboot(o) {
        var win = confirm("Are you sure you want to reboot?");
        var p = o.getAttribute("id");
        if (win == true) {
            cgiform.action = "reboot_btn.cgi";
            cgiform.submit()
        } else {
            return false
        }
    }
    function tabChange(o) {
        var url = o.getAttribute("pageid") + ".cgi";
        cgiform.action = url;
        cgiform.submit()
    }
    function UpdatePageCallback(req) {
        var pageId = req.pageId;
        var emList = document.getElementsByTagName("strong");
        for (var index = 0; index < emList.length; index++) {
            if (emList[index].getAttribute("pageid") == pageId) {
                emList[index].setAttribute("class", "cur");
                emList[index].previousElementSibling.setAttribute("class", "iconfont")
            } else {
                emList[index].setAttribute("class", "");
                emList[index].previousElementSibling.setAttribute("class", "")
            }
        }
    }
    function formatDateTime(inputTime) {
        var date = new Date(inputTime);
        var y = date.getFullYear();
        var m = date.getMonth() + 1;
        m = m < 10 ? ("0" + m) : m;
        var d = date.getDate();
        d = d < 10 ? ("0" + d) : d;
        var h = date.getHours();
        h = h < 10 ? ("0" + h) : h;
        var minute = date.getMinutes();
        var second = date.getSeconds();
        minute = minute < 10 ? ("0" + minute) : minute;
        second = second < 10 ? ("0" + second) : second;
        return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second
    }
    function minerinfoCallback(req) {
        req.hwtype !== undefined ? req.hwtype : "";
        req.mac !== undefined ? req.mac : "";
        req.ipv4 !== undefined ? req.ipv4 : "";
        req.version !== undefined ? req.version : "";
        $("hwtype").innerHTML = req.hwtype;
        $("loadtime").innerHTML = formatDateTime(new Date().getTime());
        var html = "";
        var css = "";
        if (req.sys_status == "1") {
            html = "Online";
            css = "status on"
        } else {
            html = "Idle";
            css = "status off"
        }
        $("sys_status").innerHTML = html;
        $("sys_status").setAttribute("class", css);
        $("mac").innerHTML = req.mac;
        $("ipv4").innerHTML = req.ipv4;
        $("version").innerHTML = req.version
    }
    function updateMinerInfo() {
        var oUpdate;
        oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(),
            function (t) {
                try {
                    eval(t)
                } catch (e) { }
            });
        oUpdate.doGet();
        setTimeout(updateMinerInfo, 15000)
    }
    updateMinerInfo();
    function fanSwitch(o) {
        var fan = o.getAttribute("switchid");
        if (fan == "fan_all") {
            $("fan_all").style.display = "block";
            $("fan_speed").style.display = "none"
        } else {
            $("fan_all").style.display = "none";
            $("fan_speed").style.display = "block"
        }
    }
    function tempSwitch(o) {
        var tmp = o.getAttribute("switchid");
        if (tmp == "temp1") {
            $("temp_show").style.display = "block";
            $("tempf_show").style.display = "none"
        } else {
            $("temp_show").style.display = "none";
            $("tempf_show").style.display = "block"
        }
    }
    var _i = 96;
    var _av = 0;
    var series = 0;
    var series1 = 0;
    var series2 = 0;
    function secondsFormat(s) {
        var day = Math.floor(s / (24 * 3600));
        var hour = Math.floor((s - day * 24 * 3600) / 3600);
        var minute = Math.floor((s - day * 24 * 3600 - hour * 3600) / 60);
        var second = s - day * 24 * 3600 - hour * 3600 - minute * 60;
        return {
            day: day,
            hour: hour,
            minute: minute,
            second: second
        }
    }
    var arr = ["power", "hash2", "hash1", "hash0", "fan4", "fan3", "fan2", "fan1"];
    var err = [];
    var timmer;
    var index = 0;
    function minerstatusShow() {
        if (index == err.length) index = 0;
        $("minerstatus").innerHTML = err[index];
        index++;
    }
    function homeCallback(req) {
        _hash_5m = req.hash_5m !== undefined ? req.hash_5m : 0;
        _av = req.av !== undefined ? req.av : 0;
        clearInterval(timmer);
        index = 0;

        err = [];
        var count = 0;
        var minerstatus = req.minerstatus;
        for (var i = 0; i < minerstatus.length; i++) {
            if (minerstatus.charAt(i) == "1") {
                err.push(arr[i]);
                count++;
            }
        }
        var msCls = "";
        if (count > 0) {
            msCls = "val red-round";
            minerstatusShow();
            timmer = setInterval("minerstatusShow()", 500);
        } else {
            msCls = "val green-round";
            $("minerstatus").innerHTML = "Fine";
        }
        $("minerstatus_out").setAttribute("class", msCls);

        var networkstatus = req.ping;
        var msCls = "";
        var msVal = "";
        if (networkstatus == "0") {
            msCls = "val red-round";
            msVal = networkstatus
        } else {
            msCls = "val green-round";
            msVal = networkstatus
        }
        $("networkstatus_out").setAttribute("class", msCls);
        $("networkstatus").innerHTML = msVal;

        $("fanr").innerHTML = req.fanr + "%";
        var fan1 = req.fan1;
        var fan2 = req.fan2;
        var fan3 = req.fan3;
        var fan4 = req.fan4;
        $("fan1").innerHTML = fan1;
        $("fan2").innerHTML = fan2;
        $("fan3").innerHTML = fan3;
        $("fan4").innerHTML = fan4;
        var msCls = "";
        if (fan1 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan1").setAttribute("class", msCls);
        if (fan2 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan2").setAttribute("class", msCls);
        if (fan3 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan3").setAttribute("class", msCls);
        if (fan4 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan4").setAttribute("class", msCls);

        var temp = req.temperature;
        var temp1 = req.MTavg1;
        var temp2 = req.MTavg2;
        var temp3 = req.MTavg3;
        var tempf = req.temperaturef;
        var temp1f = req.MTavg1f;
        var temp2f = req.MTavg2f;
        var temp3f = req.MTavg3f;

        $("temp").innerHTML = temp + "°C(T)";
        $("temp1").innerHTML = temp1 + "°C(H0)";
        $("temp2").innerHTML = temp2 + "°C(H1)";
        $("temp3").innerHTML = temp3 + "°C(H2)";
        $("tempf").innerHTML = tempf + "°F(T)";
        $("temp1f").innerHTML = temp1f + "°F(H0)";
        $("temp2f").innerHTML = temp2f + "°F(H1)";
        $("temp3f").innerHTML = temp3f + "°F(H2)";

        var tVal = parseInt(temp);
        var tVal1 = parseInt(temp1);
        var tVal2 = parseInt(temp2);
        var tVal3 = parseInt(temp3);
        var msCls = "";
        if (tVal <= 0 || tVal >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp").setAttribute("class", msCls);
        $("tempf").setAttribute("class", msCls);
        if (tVal1 <= 0 || tVal1 >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp1").setAttribute("class", msCls);
        $("temp1f").setAttribute("class", msCls);
        if (tVal2 <= 0 || tVal2 >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp2").setAttribute("class", msCls);
        $("temp2f").setAttribute("class", msCls);
        if (tVal3 <= 0 || tVal3 >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp3").setAttribute("class", msCls);
        $("temp3f").setAttribute("class", msCls);

        $("realtime").innerHTML = req.hash_5m;
        $("av").innerHTML = req.av;
        $("rejectedp").innerHTML = req.rejected_percentage;
        var elapsed = req.elapsed;
        var s = parseInt(req.elapsed);
        if (s < 0) {
            s = 0
        }
        var time = secondsFormat(s);
        $("day").innerHTML = time.day;
        $("hour").innerHTML = time.hour;
        $("minute").innerHTML = time.minute;
        $("second").innerHTML = time.second;
        $("url").innerHTML = req.url;
        $("worker").innerHTML = req.worker;
        var accepted = $("accepted").innerHTML;
        var rejected = $("rejected").innerHTML;
        if (accepted != req.accepted && rejected != req.reject) {
            $("accepted").innerHTML = req.accepted;
            $("rejected").innerHTML = req.reject;
            var accepted = parseInt(req.accepted);
            var rejected = parseInt(req.reject);
            loadPieData(accepted, rejected)
        }
        var x = (new Date()).getTime();
        var y = parseFloat(_hash_5m);
        var y1 = parseFloat(_av);
        series.addPoint([x, y], true, true);
        series1.addPoint([x, y1], true, true)
    }

    function updateHome() {
        var oUpdate;
        oUpdate = new AJAX("get_home.cgi?num=" + Math.random(),
            function (t) {
                try {
                    eval(t)
                } catch (e) {
                    console.log("updateHome err:" + e.message)
                }
            });
        oUpdate.doGet();
        setTimeout(updateHome, 15000)
    }
    updateHome();
    var linechart;
    var lineOptions = {
        chart: {
            renderTo: "linechart",
            type: "spline",
            events: {
                load: function () {
                    series = this.series[0];
                    series1 = this.series[1]
                }
            },
            spacing: [10, 300, 15, 10],
        },
        title: {
            text: "Computing monitoring"
        },
        xAxis: {
            type: "datetime",
            tickPixelInterval: 50
        },
        credits: {
            enabled: false
        },
        yAxis: [{
            tickPositions: [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            labels: {
                formatter: function () {
                    return this.value + "THs"
                },
                style: {
                    color: "#7cb5ec"
                }
            },
            title: {
                text: "THS",
                style: {
                    color: "#7cb5ec"
                }
            },
        }],
        colors: ["#ee8a96", "#bfbf00", "#06C", "#036", "#000"],
        legend: {
            align: "right",
            layout: "vertical",
            x: 100,
            floating: true,
            itemMarginBottom: 30,
        },
        exporting: {
            enabled: false
        },
        series: [{
            name: "Real-time",
            data: (function () {
                var data = [],
                    time = (new Date()).getTime(),
                    i;
                for (i = -_i; i <= 0; i++) {
                    data.push({
                        x: time + i * 1000,
                        y: null
                    })
                }
                return data
            })(),
            marker: {
                enabled: false
            },
        },
        {
            name: "Average",
            data: (function () {
                var data = [],
                    time = (new Date()).getTime(),
                    i;
                for (i = -_i; i <= 0; i++) {
                    data.push({
                        x: time + i * 1000,
                        y: null
                    })
                }
                return data
            })(),
            marker: {
                enabled: false
            },
        }]
    };
    linechart = new Highcharts.Chart(lineOptions);
    var piechart;
    var options = {
        chart: {
            renderTo: "piechart",
            type: "pie",
            backgroundColor: "rgba(0,0,0,0)"
        },
        credits: {
            enabled: false
        },
        title: {
            text: ""
        },
        plotOptions: {
            pie: {
                slicedOffset: 0,
                dataLabels: {
                    format: "<b>{point.name}</b><br> {point.percentage:.1f} %",
                    distance: -45,
                    style: {
                        textOutline: "1px 1px #130c0e",
                        fontSize: "12px",
                        color: "#FFFFFF"
                    }
                },
                startAngle: -90,
                endAngle: -90,
            }
        },
        colors: ["#1d953f", "#ed1941"],
        series: [{
            name: "pool",
            data: [{
                name: "Accepted",
                y: 0
            },
            {
                name: "Rejected",
                y: 0
            }]
        }]
    };
    function loadPieData(accepted, rejected) {
        options.series[0].data = [{
            name: "Accepted",
            y: accepted
        },
        {
            name: "Rejected",
            y: rejected
        }];
        piechart = new Highcharts.Chart(options)
    };


// Offset: 0x0E0431

    function checkLogin(){ 
    	if (loginform.username.value == "") { 
    		alert("Username is null!");
    		return false;
    	}
    	if(loginform.passwd.value == ""){ 
			alert("Password is null!");
    		return false;
    	}
    	loginform.submit();
    }      
    

// Offset: 0x26DCF1

  (function (doc, win) { var docEl = doc.documentElement, resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize", recalc = function () { var clientWidth = docEl.clientWidth; if (!clientWidth) { return } if (clientWidth >= 750) { docEl.style.fontSize = "100px" } else { docEl.style.fontSize = 100 * (clientWidth / 750) + "px" } }; if (!doc.addEventListener) { return } win.addEventListener(resizeEvt, recalc, false); doc.addEventListener("DOMContentLoaded", recalc, false) })(document, window); function $(id) { return document.getElementById(id) } function $$(id) { return document.getElementsByName(id) } function AJAX(url, callback) { var req = AJAX_init(); req.onreadystatechange = AJAX_processRequest; function AJAX_init() { if (window.XMLHttpRequest) { return new XMLHttpRequest() } else { if (window.ActiveXObject) { return new ActiveXObject("Microsoft.XMLHTTP") } } } function AJAX_processRequest() { if (req.readyState == 4) { if (req.status == 200) { if (callback) { callback(req.responseText) } } } } this.doGet = function () { req.open("GET", url, true); req.send(null) }; this.doPost = function (body) { req.open("POST", url, true); req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded"); req.setRequestHeader("ISAJAX", "yes"); req.send(body) } } (function () { var navWrap = document.getElementById("side_nav"); var nav1s = navWrap.getElementsByTagName("li"); var nav2s = navWrap.getElementsByTagName("ul"); for (var i = 0, len = nav1s.length; i < len; i++) { nav1s[i].onclick = (function (i) { return function () { if (nav2s[i].style.display == "none") { nav2s[i].style.display = "block" } else { nav2s[i].style.display = "none" } } })(i) } })(); function logout(o) { var win = confirm("Are you sure you want to logout?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "logout.cgi"; cgiform.submit() } else { return false } } function reboot(o) { var win = confirm("Are you sure you want to reboot?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "reboot_btn.cgi"; cgiform.submit() } else { return false } } function tabChange(o) { var url = o.getAttribute("pageid") + ".cgi"; cgiform.action = url; cgiform.submit() } function UpdatePageCallback(req) { var pageId = req.pageId; var emList = document.getElementsByTagName("strong"); for (var index = 0; index < emList.length; index++) { if (emList[index].getAttribute("pageid") == pageId) { emList[index].setAttribute("class", "cur"); emList[index].previousElementSibling.setAttribute("class", "iconfont") } else { emList[index].setAttribute("class", ""); emList[index].previousElementSibling.setAttribute("class", "") } } } function formatDateTime(inputTime) { var date = new Date(inputTime); var y = date.getFullYear(); var m = date.getMonth() + 1; m = m < 10 ? ("0" + m) : m; var d = date.getDate(); d = d < 10 ? ("0" + d) : d; var h = date.getHours(); h = h < 10 ? ("0" + h) : h; var minute = date.getMinutes(); var second = date.getSeconds(); minute = minute < 10 ? ("0" + minute) : minute; second = second < 10 ? ("0" + second) : second; return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second } function minerinfoCallback(req) { req.hwtype !== undefined ? req.hwtype : ""; req.mac !== undefined ? req.mac : ""; req.ipv4 !== undefined ? req.ipv4 : ""; req.version !== undefined ? req.version : ""; $("hwtype").innerHTML = req.hwtype; $("loadtime").innerHTML = formatDateTime(new Date().getTime()); var html = ""; var css = ""; if (req.sys_status == "1") { html = "Online"; css = "status on" } else { html = "Idle"; css = "status off" } $("sys_status").innerHTML = html; $("sys_status").setAttribute("class", css); $("mac").innerHTML = req.mac; $("ipv4").innerHTML = req.ipv4; $("version").innerHTML = req.version } function updateMinerInfo() { var oUpdate; oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet(); setTimeout(updateMinerInfo, 15000) } updateMinerInfo(); function updateLog() { var oUpdate; oUpdate = new AJAX("updatecglog.cgi?num=" + Math.random(), function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet(); setTimeout(updateLog, 5000) } function DebugSwitch(o) { var p = o.name; var oDebug = new AJAX("cglog.cgi", function (t) { try { eval(t) } catch (e) { } }); oDebug.doPost("debugswitchbtn=" + p) } function CgLogCallback(req) { console.log("log==" + req.cglog); $("cglog").innerHTML = req.cglog } updateLog();


// Offset: 0x2750B4

  (function (doc, win) {
    var docEl = doc.documentElement,
      resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize",
      recalc = function () {
        var clientWidth = docEl.clientWidth;
        if (!clientWidth) {
          return
        }
        if (clientWidth >= 750) {
          docEl.style.fontSize = "100px"
        } else {
          docEl.style.fontSize = 100 * (clientWidth / 750) + "px"
        }
      };
    if (!doc.addEventListener) {
      return
    }
    win.addEventListener(resizeEvt, recalc, false);
    doc.addEventListener("DOMContentLoaded", recalc, false)
  })(document, window);
  function $(id) {
    return document.getElementById(id)
  }
  function $$(id) {
    return document.getElementsByName(id)
  }
  function AJAX(url, callback) {
    var req = AJAX_init();
    req.onreadystatechange = AJAX_processRequest;
    function AJAX_init() {
      if (window.XMLHttpRequest) {
        return new XMLHttpRequest()
      } else {
        if (window.ActiveXObject) {
          return new ActiveXObject("Microsoft.XMLHTTP")
        }
      }
    }
    function AJAX_processRequest() {
      if (req.readyState == 4) {
        if (req.status == 200) {
          if (callback) {
            callback(req.responseText)
          }
        }
      }
    }
    this.doGet = function () {
      req.open("GET", url, true);
      req.send(null)
    };
    this.doPost = function (body) {
      req.open("POST", url, true);
      req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      req.setRequestHeader("ISAJAX", "yes");
      req.send(body)
    }
  } (function () {
    var navWrap = document.getElementById("side_nav");
    var nav1s = navWrap.getElementsByTagName("li");
    var nav2s = navWrap.getElementsByTagName("ul");
    for (var i = 0,
      len = nav1s.length; i < len; i++) {
      nav1s[i].onclick = (function (i) {
        return function () {
          if (nav2s[i].style.display == "none") {
            nav2s[i].style.display = "block"
          } else {
            nav2s[i].style.display = "none"
          }
        }
      })(i)
    }
  })();
  function logout(o) {
    var win = confirm("Are you sure you want to logout?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "logout.cgi";
      cgiform.submit()
    } else {
      return false
    }
  }
  function reboot(o) {
    var win = confirm("Are you sure you want to reboot?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "reboot_btn.cgi";
      cgiform.submit()
    } else {
      return false
    }
  }
  function tabChange(o) {
    var url = o.getAttribute("pageid") + ".cgi";
    cgiform.action = url;
    cgiform.submit()
  }
  function UpdatePageCallback(req) {
    var pageId = req.pageId;
    var emList = document.getElementsByTagName("strong");
    for (var index = 0; index < emList.length; index++) {
      if (emList[index].getAttribute("pageid") == pageId) {
        emList[index].setAttribute("class", "cur");
        emList[index].previousElementSibling.setAttribute("class", "iconfont")
      } else {
        emList[index].setAttribute("class", "");
        emList[index].previousElementSibling.setAttribute("class", "")
      }
    }
  }
  function formatDateTime(inputTime) {
    var date = new Date(inputTime);
    var y = date.getFullYear();
    var m = date.getMonth() + 1;
    m = m < 10 ? ("0" + m) : m;
    var d = date.getDate();
    d = d < 10 ? ("0" + d) : d;
    var h = date.getHours();
    h = h < 10 ? ("0" + h) : h;
    var minute = date.getMinutes();
    var second = date.getSeconds();
    minute = minute < 10 ? ("0" + minute) : minute;
    second = second < 10 ? ("0" + second) : second;
    return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second
  }
  function minerinfoCallback(req) {
    req.hwtype !== undefined ? req.hwtype : "";
    req.mac !== undefined ? req.mac : "";
    req.ipv4 !== undefined ? req.ipv4 : "";
    req.version !== undefined ? req.version : "";
    $("hwtype").innerHTML = req.hwtype;
    $("loadtime").innerHTML = formatDateTime(new Date().getTime());
    var html = "";
    var css = "";
    if (req.sys_status == "1") {
      html = "Online";
      css = "status on"
    } else {
      html = "Idle";
      css = "status off"
    }
    $("sys_status").innerHTML = html;
    $("sys_status").setAttribute("class", css);
    $("mac").innerHTML = req.mac;
    $("ipv4").innerHTML = req.ipv4;
    $("version").innerHTML = req.version
  }
  function updateMinerInfo() {
    var oUpdate;
    oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(),
      function (t) {
        try {
          eval(t)
        } catch (e) { }
      });
    oUpdate.doGet();
    setTimeout(updateMinerInfo, 15000)
  }
  updateMinerInfo();
  function updateCGConf() {
    var oUpdate;
    oUpdate = new AJAX("updatecgconf.cgi",
      function (t) {
        try {
          eval(t)
        } catch (e) { }
      });
    oUpdate.doGet()
  }
  function selset(id, val) {
    var o = $(id);
    for (var i = 0; i < o.options.length; i++) {
      if (i == val) {
        o.options[i].selected = true;
        break
      }
    }
  }
  function CGConfCallback(req) {
    $("worker1").value = req.worker1;
    $("passwd1").value = req.passwd1;
    $("worker2").value = req.worker2;
    $("passwd2").value = req.passwd2;
    $("worker3").value = req.worker3;
    $("passwd3").value = req.passwd3;
    $("pool1").value = req.pool1;
    $("pool2").value = req.pool2;
    $("pool3").value = req.pool3;
    selset("mode", req.mode)
  }
  function confChk(o) {
    var len = o.value.length;
    var pattern = /(^[0-9.]+$)/;
    if (!pattern.test(o.value)) {
      alert("Allow only numbers and dot(.)");
      o.value = o.value.substring(0, (o.value.length - 1))
    }
  }
  function submitcgminercfg(o) {
    cgconfform.submit();
    alert("Save successfully, Need reboot manualy")
  }
  updateCGConf();


// Offset: 0x27BBC8

    (function (doc, win) { var docEl = doc.documentElement, resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize", recalc = function () { var clientWidth = docEl.clientWidth; if (!clientWidth) { return } if (clientWidth >= 750) { docEl.style.fontSize = "100px" } else { docEl.style.fontSize = 100 * (clientWidth / 750) + "px" } }; if (!doc.addEventListener) { return } win.addEventListener(resizeEvt, recalc, false); doc.addEventListener("DOMContentLoaded", recalc, false) })(document, window); function $(id) { return document.getElementById(id) } function $$(id) { return document.getElementsByName(id) } function AJAX(url, callback) { var req = AJAX_init(); req.onreadystatechange = AJAX_processRequest; function AJAX_init() { if (window.XMLHttpRequest) { return new XMLHttpRequest() } else { if (window.ActiveXObject) { return new ActiveXObject("Microsoft.XMLHTTP") } } } function AJAX_processRequest() { if (req.readyState == 4) { if (req.status == 200) { if (callback) { callback(req.responseText) } } } } this.doGet = function () { req.open("GET", url, true); req.send(null) }; this.doPost = function (body) { req.open("POST", url, true); req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded"); req.setRequestHeader("ISAJAX", "yes"); req.send(body) } } (function () { var navWrap = document.getElementById("side_nav"); var nav1s = navWrap.getElementsByTagName("li"); var nav2s = navWrap.getElementsByTagName("ul"); for (var i = 0, len = nav1s.length; i < len; i++) { nav1s[i].onclick = (function (i) { return function () { if (nav2s[i].style.display == "none") { nav2s[i].style.display = "block" } else { nav2s[i].style.display = "none" } } })(i) } })(); function logout(o) { var win = confirm("Are you sure you want to logout?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "logout.cgi"; cgiform.submit() } else { return false } } function reboot(o) { var win = confirm("Are you sure you want to reboot?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "reboot_btn.cgi"; cgiform.submit() } else { return false } } function tabChange(o) { var url = o.getAttribute("pageid") + ".cgi"; cgiform.action = url; cgiform.submit() } function UpdatePageCallback(req) { var pageId = req.pageId; var emList = document.getElementsByTagName("strong"); for (var index = 0; index < emList.length; index++) { if (emList[index].getAttribute("pageid") == pageId) { emList[index].setAttribute("class", "cur"); emList[index].previousElementSibling.setAttribute("class", "iconfont") } else { emList[index].setAttribute("class", ""); emList[index].previousElementSibling.setAttribute("class", "") } } } function formatDateTime(inputTime) { var date = new Date(inputTime); var y = date.getFullYear(); var m = date.getMonth() + 1; m = m < 10 ? ("0" + m) : m; var d = date.getDate(); d = d < 10 ? ("0" + d) : d; var h = date.getHours(); h = h < 10 ? ("0" + h) : h; var minute = date.getMinutes(); var second = date.getSeconds(); minute = minute < 10 ? ("0" + minute) : minute; second = second < 10 ? ("0" + second) : second; return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second } function minerinfoCallback(req) { req.hwtype !== undefined ? req.hwtype : ""; req.mac !== undefined ? req.mac : ""; req.ipv4 !== undefined ? req.ipv4 : ""; req.version !== undefined ? req.version : ""; $("hwtype").innerHTML = req.hwtype; $("loadtime").innerHTML = formatDateTime(new Date().getTime()); var html = ""; var css = ""; if (req.sys_status == "1") { html = "Online"; css = "status on" } else { html = "Idle"; css = "status off" } $("sys_status").innerHTML = html; $("sys_status").setAttribute("class", css); $("mac").innerHTML = req.mac; $("ipv4").innerHTML = req.ipv4; $("version").innerHTML = req.version } function updateMinerInfo() { var oUpdate; oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet(); setTimeout(updateMinerInfo, 15000) } updateMinerInfo(); function changePasswd() { if (adminform.passwd.value == "") { alert("New Password is not null!"); return false } if (adminform.confirm.value == "") { alert("Confirm is not null!"); return false } if (adminform.passwd.value != adminform.confirm.value) { alert("Password and confirm password disagree"); return false } adminform.submit() } function updateadmin() { var oUpdate; oUpdate = new AJAX("updateadmin.cgi", function (t) { try { eval(t) } catch (e) { } }); oUpdate.doGet() } updateadmin();


// Offset: 0x28033B

    function checkLogin(){ 
    	if (loginform.username.value == "") { 
    		alert("Username is null!");
    		return false;
    	}
    	if(loginform.passwd.value == ""){ 
			alert("Password is null!");
    		return false;
    	}
    	loginform.submit();
    }      
    

// Offset: 0x280716

            var i = 20;
            var intervalid;
            intervalid = setInterval("fun()", 1000);

            function fun() {
                if (i == 0) {
                    window.location.href = "/";
                    clearInterval(intervalid)
                }
                document.getElementById("mes").innerHTML = i;
                i--
            }
        

// Offset: 0x28097C

            var xhr = new XMLHttpRequest();
            xhr.open('POST', 'reboot.cgi');
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.onload = function () { };
            xhr.send(encodeURI('reboot=1'));
        

// Offset: 0x286DF0

  (function (doc, win) {
    var docEl = doc.documentElement,
      resizeEvt =
        "onorientationchange" in window ? "onorientationchange" : "resize",
      recalc = function () {
        var clientWidth = docEl.clientWidth;
        if (!clientWidth) {
          return;
        }
        if (clientWidth >= 750) {
          docEl.style.fontSize = "100px";
        } else {
          docEl.style.fontSize = 100 * (clientWidth / 750) + "px";
        }
      };
    if (!doc.addEventListener) {
      return;
    }
    win.addEventListener(resizeEvt, recalc, false);
    doc.addEventListener("DOMContentLoaded", recalc, false);
  })(document, window);
  function $(id) {
    return document.getElementById(id);
  }
  function $$(id) {
    return document.getElementsByName(id);
  }
  function AJAX(url, callback) {
    var req = AJAX_init();
    req.onreadystatechange = AJAX_processRequest;
    function AJAX_init() {
      if (window.XMLHttpRequest) {
        return new XMLHttpRequest();
      } else {
        if (window.ActiveXObject) {
          return new ActiveXObject("Microsoft.XMLHTTP");
        }
      }
    }
    function AJAX_processRequest() {
      if (req.readyState == 4) {
        if (req.status == 200) {
          if (callback) {
            callback(req.responseText);
          }
        }
      }
    }
    this.doGet = function () {
      req.open("GET", url, true);
      req.send(null);
    };
    this.doPost = function (body) {
      req.open("POST", url, true);
      req.setRequestHeader(
        "Content-Type",
        "application/x-www-form-urlencoded"
      );
      req.setRequestHeader("ISAJAX", "yes");
      req.send(body);
    };
  }
  (function () {
    var navWrap = document.getElementById("side_nav");
    var nav1s = navWrap.getElementsByTagName("li");
    var nav2s = navWrap.getElementsByTagName("ul");
    for (var i = 0, len = nav1s.length; i < len; i++) {
      nav1s[i].onclick = (function (i) {
        return function () {
          if (nav2s[i].style.display == "none") {
            nav2s[i].style.display = "block";
          } else {
            nav2s[i].style.display = "none";
          }
        };
      })(i);
    }
  })();
  function logout(o) {
    var win = confirm("Are you sure you want to logout?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "logout.cgi";
      cgiform.submit();
    } else {
      return false;
    }
  }
  function reboot(o) {
    var win = confirm("Are you sure you want to reboot?");
    var p = o.getAttribute("id");
    if (win == true) {
      cgiform.action = "reboot_btn.cgi";
      cgiform.submit();
    } else {
      return false;
    }
  }
  function tabChange(o) {
    var url = o.getAttribute("pageid") + ".cgi";
    cgiform.action = url;
    cgiform.submit();
  }
  function UpdatePageCallback(req) {
    var pageId = req.pageId;
    var emList = document.getElementsByTagName("strong");
    for (var index = 0; index < emList.length; index++) {
      if (emList[index].getAttribute("pageid") == pageId) {
        emList[index].setAttribute("class", "cur");
        emList[index].previousElementSibling.setAttribute(
          "class",
          "iconfont"
        );
      } else {
        emList[index].setAttribute("class", "");
        emList[index].previousElementSibling.setAttribute("class", "");
      }
    }
  }
  function formatDateTime(inputTime) {
    var date = new Date(inputTime);
    var y = date.getFullYear();
    var m = date.getMonth() + 1;
    m = m < 10 ? "0" + m : m;
    var d = date.getDate();
    d = d < 10 ? "0" + d : d;
    var h = date.getHours();
    h = h < 10 ? "0" + h : h;
    var minute = date.getMinutes();
    var second = date.getSeconds();
    minute = minute < 10 ? "0" + minute : minute;
    second = second < 10 ? "0" + second : second;
    return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second;
  }
  function minerinfoCallback(req) {
    req.hwtype !== undefined ? req.hwtype : "";
    req.mac !== undefined ? req.mac : "";
    req.ipv4 !== undefined ? req.ipv4 : "";
    req.version !== undefined ? req.version : "";
    req.stm8 !== undefined ? req.stm8 : "";
    $("hwtype").innerHTML = req.hwtype;
    $("loadtime").innerHTML = formatDateTime(new Date().getTime());
    var html = "";
    var css = "";
    if (req.sys_status == "1") {
      html = "Online";
      css = "status on";
    } else {
      html = "Idle";
      css = "status off";
    }
    $("sys_status").innerHTML = html;
    $("sys_status").setAttribute("class", css);
    $("mac").innerHTML = req.mac;
    $("ipv4").innerHTML = req.ipv4;
    $("version").innerHTML = req.version;
    stm8 = req.stm8;
  }

  function updateMinerInfo() {
    var oUpdate;
    oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (
      t
    ) {
      try {
        eval(t);
      } catch (e) { }
    });
    oUpdate.doGet();
    setTimeout(updateMinerInfo, 15000);
  }
  updateMinerInfo();
  function decToHex(dec, bytes) {
    hex = dec.toString(16);
    while (hex.length < bytes) {
      hex = "0" + hex;
    }
    return hex;
  }
  function hex_to_ascii(str) {
    var arr = [];
    for (var i = 0, l = str.length; i < l; i++) {
      var hex = Number(str.charCodeAt(i)).toString(16);
      if (hex.length < 2) {
        hex = "0" + hex;
      }
      arr.push(hex);
    }
    return arr.join("");
  }
  var pkg_num = 0;
  var offset = 0;
  var reply;
  var files;
  var timer;
  var begin_timer;
  var repeat_cnt = 0;
  var end = 0;
  var flash_time = 120;
  var intervalid;
  var _width = 0;
  var progress_timer;
  var stm8;
  FileReader.prototype.readAsBinaryString = function (fileData) {
    var binary = "";
    var pt = this;
    var reader = new FileReader();
    reader.onload = function (e) {
      var bytes = new Uint8Array(reader.result);
      var length = bytes.byteLength;
      for (var i = 0; i < length; i++) {
        binary += String.fromCharCode(bytes[i]);
      }
      pt.content = binary;
      pt.onload(pt);
    };
    reader.readAsArrayBuffer(fileData);
  };
  function load_data() {
    var cgi_reply = document.getElementById("cont").innerHTML;
    if (cgi_reply == "OK" + decToHex(pkg_num, 4)) {
      pkg_num = pkg_num + 1;
      offset = offset + 256;
      repeat_cnt = 0;
      document.getElementById("cont").innerHTML = "0";
    }
    var reader = new FileReader();
    end = offset + 256 < files[0].size ? offset + 256 : files[0].size;
    reader.readAsBinaryString(files[0].slice(offset, end));
    reader.onload = function (file) {
      var buf_index = 0;
      var pkg_len = file.content.length;
      var oPost = new AJAX("upload.cgi?num=" + Math.random(), function (t) {
        try {
        } catch (e) {
          alert(e);
        }
        reply = t;
        document.getElementById("cont").innerHTML = reply;
      });
      oPost.doPost(
        "uploadbtn=" +
        decToHex(parseInt(pkg_num >> 8), 2) +
        decToHex(parseInt(pkg_num & 255), 2) +
        decToHex(parseInt(pkg_len >> 8), 2) +
        decToHex(parseInt(pkg_len & 255), 2) +
        decToHex(repeat_cnt & 255, 2) +
        hex_to_ascii(file.content)
      );
    };
    timer = setTimeout(load_data, 60);
    if (offset >= files[0].size) {
      clearTimeout(timer);
      end_cmd();
    }
    repeat_cnt = repeat_cnt + 1;
    if (repeat_cnt > 512) {
      repeat_cnt = 0;
      clearTimeout(timer);
      alert("send  img packet timeout");
    }
  }
  function begin_cmd(img_len) {
    var oPost = new AJAX("upload.cgi?num=" + Math.random(), function (t) {
      try {
      } catch (e) {
        alert(e);
      }
      document.getElementById("cont").innerHTML = t;
    });
    img_len = files[0].size;
    oPost.doPost(
      "begin=" +
      decToHex(parseInt(img_len >> 24) & 255, 2) +
      decToHex(parseInt(img_len >> 16) & 255, 2) +
      decToHex(parseInt(img_len >> 8) & 255, 2) +
      decToHex(parseInt(img_len & 255), 2)
    );
    begin_timer = setTimeout(begin_cmd, 100);
    if (document.getElementById("cont").innerHTML == "OK") {
      clearTimeout(begin_timer);
      document.getElementById("cont").innerHTML = "0";
      load_data();
    }
    if (document.getElementById("cont").innerHTML == "ERR-0") {
      document.getElementById("cont").innerHTML = "0";
      clearTimeout(begin_timer);
      alert("image file too bigger");
    }
  }
  function end_cmd() {
    var oPost = new AJAX("upload.cgi?num=" + Math.random(), function (t) {
      try {
      } catch (e) {
        alert(e);
      }
      document.getElementById("cont").innerHTML = t;
    });
    oPost.doPost("end=");
  }
  function upgrade_confim(rep) {
    var win = confirm(
      "Image File Information:\r\n" +
      "name:" +
      rep.filename +
      "\r\nsize:" +
      rep.size
    );
    if (win == true) {
      load_progress();
      begin_cmd(rep.size);
      showAndHidden(0);
    } else {
      return false;
    }
  }
  function UploadFirmware(o) {
    var p = o.name;
    var img_info;
    var files_source = document.getElementById("firmware");
    files = files_source.files;
    pkg_num = 0;
    offset = 0;
    document.getElementById("cont").innerHTML = "0";
    img_info = { filename: files[0].name, size: files[0].size };
    //var stm8 = document.getElementById("stm8").value;
    if(typeof(stm8)!="undefined"&&stm8!=null&&stm8!="") {
      if(img_info.filename.toLowerCase().indexOf("plus") == -1){
        alert("this device contains property stm8, upgrade file must be plus version!");
        return;
      }
    } else {
      if(img_info.filename.toLowerCase().indexOf("plus") != -1){
        alert("this device don't contains property stm8, upgrade file must not be plus version!");
        return;
      }
    }
    upgrade_confim(img_info);
  }
  function updateupgrade() {
    var oUpdate;
    oUpdate = new AJAX("updateupgrade.cgi", function (t) {
      try {
        eval(t);
      } catch (e) { }
    });
    oUpdate.doGet();
  }
  function get_upgrade_result() {
    var oUpdate;
    oUpdate = new AJAX("upgrade_status.cgi?num=" + Math.random(), function (
      t
    ) {
      try {
        eval(t);
      } catch (e) { }
      document.getElementById("result").innerHTML = t;
    });
    oUpdate.doGet();
  }
  function load_progress() {
    progress_timer = setTimeout(load_progress, 200);
    _width = (offset / files[0].size) * 490;
    if (_width >= 490) {
      clearTimeout(progress_timer);
      showAndHidden(1);
    }
    document.getElementsByClassName("inner")[0].style.width = _width + "px";
    var _data = Math.floor((_width / 490) * 100);
    document.getElementsByClassName("data")[0].innerText = _data + "%";
    document.getElementsByClassName("data")[0].style.left =
      256 + _width + "px";
  }
  function burn_progress() {
    get_upgrade_result();
    intervalid = setTimeout(burn_progress, 1000);
    if (
      document.getElementById("result").innerHTML == 7 ||
      document.getElementById("result").innerHTML == 8 ||
      document.getElementById("result").innerHTML == "OK"
    ) {
      clearTimeout(intervalid);
      if (document.getElementById("result").innerHTML == "OK") {
        clearTimeout(intervalid);
        document.getElementById("mes").innerHTML =
          "Upgrade successully ,please reboot device manully";
      } else {
        alert("Upgrade faild");
        document.getElementById("mes").innerHTML = "Upgrade faild";
      }
    } else {
      if (flash_time == 0) {
        clearTimeout(intervalid);
        document.getElementById("mes").innerHTML = "Program flash timeout";
      } else {
        document.getElementById("mes").innerHTML =
          "Program flash..., will be finished after (" +
          flash_time +
          ") second";
        flash_time--;
      }
    }
  }
  function showAndHidden(divflag) {
    var div1 = document.getElementById("div1");
    var div2 = document.getElementById("div2");
    var div3 = document.getElementById("div3");
    if (divflag == 1) {
      div1.style.display = "none";
      div2.style.display = "none";
      div3.style.display = "block";
      flash_time = 120;
      burn_progress();
    } else {
      if (div1.style.display == "block") {
        div1.style.display = "none";
      } else {
        div1.style.display = "block";
      }
      if (div2.style.display == "block") {
        div2.style.display = "none";
      } else {
        div2.style.display = "block";
      }
      div3.style.display = "none";
    }
  }
  updateupgrade();


// Offset: 0x28EEE1
(function(doc,win){var docEl=doc.documentElement,resizeEvt="onorientationchange" in window?"onorientationchange":"resize",recalc=function(){var clientWidth=docEl.clientWidth;if(!clientWidth){return}if(clientWidth>=750){docEl.style.fontSize="100px"}else{docEl.style.fontSize=100*(clientWidth/750)+"px"}};if(!doc.addEventListener){return}win.addEventListener(resizeEvt,recalc,false);doc.addEventListener("DOMContentLoaded",recalc,false)})(document,window);function $(id){return document.getElementById(id)}function $$(id){return document.getElementsByName(id)}function AJAX(url,callback){var req=AJAX_init();req.onreadystatechange=AJAX_processRequest;function AJAX_init(){if(window.XMLHttpRequest){return new XMLHttpRequest()}else{if(window.ActiveXObject){return new ActiveXObject("Microsoft.XMLHTTP")}}}function AJAX_processRequest(){if(req.readyState==4){if(req.status==200){if(callback){callback(req.responseText)}}}}this.doGet=function(){req.open("GET",url,true);req.send(null)};this.doPost=function(body){req.open("POST",url,true);req.setRequestHeader("Content-Type","application/x-www-form-urlencoded");req.setRequestHeader("ISAJAX","yes");req.send(body)}}(function(){var navWrap=document.getElementById("side_nav");var nav1s=navWrap.getElementsByTagName("li");var nav2s=navWrap.getElementsByTagName("ul");for(var i=0,len=nav1s.length;i<len;i++){nav1s[i].onclick=(function(i){return function(){if(nav2s[i].style.display=="none"){nav2s[i].style.display="block"}else{nav2s[i].style.display="none"}}})(i)}})();function logout(o){var win=confirm("Are you sure you want to logout?");var p=o.getAttribute("id");if(win==true){cgiform.action="logout.cgi";cgiform.submit()}else{return false}}function reboot(o){var win=confirm("Are you sure you want to reboot?");var p=o.getAttribute("id");if(win==true){cgiform.action="reboot_btn.cgi";cgiform.submit()}else{return false}}function tabChange(o){var url=o.getAttribute("pageid")+".cgi";cgiform.action=url;cgiform.submit()}function UpdatePageCallback(req){var pageId=req.pageId;var emList=document.getElementsByTagName("strong");for(var index=0;index<emList.length;index++){if(emList[index].getAttribute("pageid")==pageId){emList[index].setAttribute("class","cur");emList[index].previousElementSibling.setAttribute("class","iconfont")}else{emList[index].setAttribute("class","");emList[index].previousElementSibling.setAttribute("class","")}}}function formatDateTime(inputTime){var date=new Date(inputTime);var y=date.getFullYear();var m=date.getMonth()+1;m=m<10?("0"+m):m;var d=date.getDate();d=d<10?("0"+d):d;var h=date.getHours();h=h<10?("0"+h):h;var minute=date.getMinutes();var second=date.getSeconds();minute=minute<10?("0"+minute):minute;second=second<10?("0"+second):second;return y+"-"+m+"-"+d+" "+h+":"+minute+":"+second}function minerinfoCallback(req){req.hwtype!==undefined?req.hwtype:"";req.mac!==undefined?req.mac:"";req.ipv4!==undefined?req.ipv4:"";req.version!==undefined?req.version:"";$("hwtype").innerHTML=req.hwtype;$("loadtime").innerHTML=formatDateTime(new Date().getTime());var html="";var css="";if(req.sys_status=="1"){html="Online";css="status on"}else{html="Idle";css="status off"}$("sys_status").innerHTML=html;$("sys_status").setAttribute("class",css);$("mac").innerHTML=req.mac;$("ipv4").innerHTML=req.ipv4;$("version").innerHTML=req.version}function updateMinerInfo(){var oUpdate;oUpdate=new AJAX("get_minerinfo.cgi?num="+Math.random(),function(t){try{eval(t)}catch(e){}});oUpdate.doGet();setTimeout(updateMinerInfo,15000)}updateMinerInfo();function updateNetConf(){var oUpdate;oUpdate=new AJAX("updatenetwork.cgi",function(t){try{eval(t)}catch(e){}});oUpdate.doGet()}function netinfo_block(req){if(req==0){$("ip").disabled=true;$("mask").disabled=true;$("gateway").disabled=true;$("dns").disabled=true;$("dnsbak").disabled=true}else{$("ip").disabled=false;$("mask").disabled=false;$("gateway").disabled=false;$("dns").disabled=false;$("dnsbak").disabled=false}}function NetworkCallback(req){var obj=document.getElementsByName("protocol");$("ip").value=req.ip;$("mask").value=req.mask;$("gateway").value=req.gateway;$("dns").value=req.dns;$("dnsbak").value=req.dnsbak;for(var i=0;i<obj.length;i++){if(i==req.protocal){obj[i].checked=true;break}}netinfo_block(i)}function confChk(o){var len=o.value.length;var pattern=/(^[0-9.]+$)/;if(!pattern.test(o.value)){alert("Allow only numbers and dot(.)");o.value=o.value.substring(0,(o.value.length-1))}if(len>15){o.value=o.value.substring(0,15)}}function submitNetWork(o){networkform.submit();alert("Save successfully, Need reboot manualy")}updateNetConf();

// Offset: 0x298427
(function (doc, win) {
        var docEl = doc.documentElement,
            resizeEvt = "onorientationchange" in window ? "onorientationchange" : "resize",
            recalc = function () {
                var clientWidth = docEl.clientWidth;
                if (!clientWidth) {
                    return
                }
                if (clientWidth >= 750) {
                    docEl.style.fontSize = "100px"
                } else {
                    docEl.style.fontSize = 100 * (clientWidth / 750) + "px"
                }
            };
        if (!doc.addEventListener) {
            return
        }
        win.addEventListener(resizeEvt, recalc, false);
        doc.addEventListener("DOMContentLoaded", recalc, false)
    })(document, window);
    function $(id) {
        return document.getElementById(id)
    }
    function $$(id) {
        return document.getElementsByName(id)
    }
    function addClass(id, cls) {
        var elem = document.getElementById(id);
        elem.classList.add(cls)
    }
    function removeClass(id, cls) {
        var elem = document.getElementById(id);
        elem.classList.remove(cls)
    }
    function AJAX(url, callback) {
        var req = AJAX_init();
        req.onreadystatechange = AJAX_processRequest;
        function AJAX_init() {
            if (window.XMLHttpRequest) {
                return new XMLHttpRequest()
            } else {
                if (window.ActiveXObject) {
                    return new ActiveXObject("Microsoft.XMLHTTP")
                }
            }
        }
        function AJAX_processRequest() {
            if (req.readyState == 4) {
                if (req.status == 200) {
                    if (callback) {
                        callback(req.responseText)
                    }
                }
            }
        }
        this.doGet = function () {
            req.open("GET", url, true);
            req.send(null)
        };
        this.doPost = function (body) {
            req.open("POST", url, true);
            req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            req.setRequestHeader("ISAJAX", "yes");
            req.send(body)
        }
    } (function () {
        var navWrap = document.getElementById("side_nav");
        var nav1s = navWrap.getElementsByTagName("li");
        var nav2s = navWrap.getElementsByTagName("ul");
        for (var i = 0,
            len = nav1s.length; i < len; i++) {
            nav1s[i].onclick = (function (i) {
                return function () {
                    if (nav2s[i].style.display == "none") {
                        nav2s[i].style.display = "block"
                    } else {
                        nav2s[i].style.display = "none"
                    }
                }
            })(i)
        }
    })();
    function logout(o) {
        var win = confirm("Are you sure you want to logout?");
        var p = o.getAttribute("id");
        if (win == true) {
            cgiform.action = "logout.cgi";
            cgiform.submit()
        } else {
            return false
        }
    }
    function reboot(o) {
        var win = confirm("Are you sure you want to reboot?");
        var p = o.getAttribute("id");
        if (win == true) {
            cgiform.action = "reboot_btn.cgi";
            cgiform.submit()
        } else {
            return false
        }
    }
    function tabChange(o) {
        var url = o.getAttribute("pageid") + ".cgi";
        cgiform.action = url;
        cgiform.submit()
    }
    function UpdatePageCallback(req) {
        var pageId = req.pageId;
        var emList = document.getElementsByTagName("strong");
        for (var index = 0; index < emList.length; index++) {
            if (emList[index].getAttribute("pageid") == pageId) {
                emList[index].setAttribute("class", "cur");
                emList[index].previousElementSibling.setAttribute("class", "iconfont")
            } else {
                emList[index].setAttribute("class", "");
                emList[index].previousElementSibling.setAttribute("class", "")
            }
        }
    }
    function formatDateTime(inputTime) {
        var date = new Date(inputTime);
        var y = date.getFullYear();
        var m = date.getMonth() + 1;
        m = m < 10 ? ("0" + m) : m;
        var d = date.getDate();
        d = d < 10 ? ("0" + d) : d;
        var h = date.getHours();
        h = h < 10 ? ("0" + h) : h;
        var minute = date.getMinutes();
        var second = date.getSeconds();
        minute = minute < 10 ? ("0" + minute) : minute;
        second = second < 10 ? ("0" + second) : second;
        return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second
    }
    function minerinfoCallback(req) {
        req.hwtype !== undefined ? req.hwtype : "";
        req.mac !== undefined ? req.mac : "";
        req.ipv4 !== undefined ? req.ipv4 : "";
        req.version !== undefined ? req.version : "";
        $("hwtype").innerHTML = req.hwtype;
        $("loadtime").innerHTML = formatDateTime(new Date().getTime());
        var html = "";
        var css = "";
        if (req.sys_status == "1") {
            html = "Online";
            css = "status on"
        } else {
            html = "Idle";
            css = "status off"
        }
        $("sys_status").innerHTML = html;
        $("sys_status").setAttribute("class", css);
        $("mac").innerHTML = req.mac;
        $("ipv4").innerHTML = req.ipv4;
        $("version").innerHTML = req.version
    }
    function updateMinerInfo() {
        var oUpdate;
        oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(),
            function (t) {
                try {
                    eval(t)
                } catch (e) { }
            });
        oUpdate.doGet();
        setTimeout(updateMinerInfo, 15000)
    }
    updateMinerInfo();
    function fanSwitch(o) {
        var fan = o.getAttribute("switchid");
        if (fan == "fan_all") {
            $("fan_all").style.display = "block";
            $("fan_speed").style.display = "none"
        } else {
            $("fan_all").style.display = "none";
            $("fan_speed").style.display = "block"
        }
    }
    function tempSwitch(o) {
        var tmp = o.getAttribute("switchid");
        if (tmp == "temp1") {
            $("temp_show").style.display = "block";
            $("tempf_show").style.display = "none"
        } else {
            $("temp_show").style.display = "none";
            $("tempf_show").style.display = "block"
        }
    }
    var _i = 96;
    var _av = 0;
    var series = 0;
    var series1 = 0;
    var series2 = 0;
    function secondsFormat(s) {
        var day = Math.floor(s / (24 * 3600));
        var hour = Math.floor((s - day * 24 * 3600) / 3600);
        var minute = Math.floor((s - day * 24 * 3600 - hour * 3600) / 60);
        var second = s - day * 24 * 3600 - hour * 3600 - minute * 60;
        return {
            day: day,
            hour: hour,
            minute: minute,
            second: second
        }
    }
    var arr = ["power", "hash2", "hash1", "hash0", "fan4", "fan3", "fan2", "fan1"];
    var err = [];
    var timmer;
    var index = 0;
    function minerstatusShow() {
        if (index == err.length) index = 0;
        $("minerstatus").innerHTML = err[index];
        index++;
    }
    function homeCallback(req) {
        _hash_5m = req.hash_5m !== undefined ? req.hash_5m : 0;
        _av = req.av !== undefined ? req.av : 0;
        clearInterval(timmer);
        index = 0;

        err = [];
        var count = 0;
        var minerstatus = req.minerstatus;
        for (var i = 0; i < minerstatus.length; i++) {
            if (minerstatus.charAt(i) == "1") {
                err.push(arr[i]);
                count++;
            }
        }
        var msCls = "";
        if (count > 0) {
            msCls = "val red-round";
            minerstatusShow();
            timmer = setInterval("minerstatusShow()", 500);
        } else {
            msCls = "val green-round";
            $("minerstatus").innerHTML = "Fine";
        }
        $("minerstatus_out").setAttribute("class", msCls);

        var networkstatus = req.ping;
        var msCls = "";
        var msVal = "";
        if (networkstatus == "0") {
            msCls = "val red-round";
            msVal = networkstatus
        } else {
            msCls = "val green-round";
            msVal = networkstatus
        }
        $("networkstatus_out").setAttribute("class", msCls);
        $("networkstatus").innerHTML = msVal;

        $("fanr").innerHTML = req.fanr + "%";
        var fan1 = req.fan1;
        var fan2 = req.fan2;
        var fan3 = req.fan3;
        var fan4 = req.fan4;
        $("fan1").innerHTML = fan1;
        $("fan2").innerHTML = fan2;
        $("fan3").innerHTML = fan3;
        $("fan4").innerHTML = fan4;
        var msCls = "";
        if (fan1 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan1").setAttribute("class", msCls);
        if (fan2 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan2").setAttribute("class", msCls);
        if (fan3 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan3").setAttribute("class", msCls);
        if (fan4 == "0") {
            msCls = "fan red-bg";
        } else {
            msCls = "fan green-bg";
        }
        $("fan4").setAttribute("class", msCls);

        var temp = req.temperature;
        var temp1 = req.MTavg1;
        var temp2 = req.MTavg2;
        var temp3 = req.MTavg3;
        var tempf = req.temperaturef;
        var temp1f = req.MTavg1f;
        var temp2f = req.MTavg2f;
        var temp3f = req.MTavg3f;

        $("temp").innerHTML = temp + "°C(T)";
        $("temp1").innerHTML = temp1 + "°C(H0)";
        $("temp2").innerHTML = temp2 + "°C(H1)";
        $("temp3").innerHTML = temp3 + "°C(H2)";
        $("tempf").innerHTML = tempf + "°F(T)";
        $("temp1f").innerHTML = temp1f + "°F(H0)";
        $("temp2f").innerHTML = temp2f + "°F(H1)";
        $("temp3f").innerHTML = temp3f + "°F(H2)";

        var tVal = parseInt(temp);
        var tVal1 = parseInt(temp1);
        var tVal2 = parseInt(temp2);
        var tVal3 = parseInt(temp3);
        var msCls = "";
        if (tVal <= 0 || tVal >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp").setAttribute("class", msCls);
        $("tempf").setAttribute("class", msCls);
        if (tVal1 <= 0 || tVal1 >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp1").setAttribute("class", msCls);
        $("temp1f").setAttribute("class", msCls);
        if (tVal2 <= 0 || tVal2 >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp2").setAttribute("class", msCls);
        $("temp2f").setAttribute("class", msCls);
        if (tVal3 <= 0 || tVal3 >= 90) {
            msCls = "temp red-bg"
        } else {
            msCls = "temp green-bg"
        }
        $("temp3").setAttribute("class", msCls);
        $("temp3f").setAttribute("class", msCls);

        $("realtime").innerHTML = req.hash_5m;
        $("av").innerHTML = req.av;
        $("rejectedp").innerHTML = req.rejected_percentage;
        var elapsed = req.elapsed;
        var s = parseInt(req.elapsed);
        if (s < 0) {
            s = 0
        }
        var time = secondsFormat(s);
        $("day").innerHTML = time.day;
        $("hour").innerHTML = time.hour;
        $("minute").innerHTML = time.minute;
        $("second").innerHTML = time.second;
        $("url").innerHTML = req.url;
        $("worker").innerHTML = req.worker;
        var accepted = $("accepted").innerHTML;
        var rejected = $("rejected").innerHTML;
        if (accepted != req.accepted && rejected != req.reject) {
            $("accepted").innerHTML = req.accepted;
            $("rejected").innerHTML = req.reject;
            var accepted = parseInt(req.accepted);
            var rejected = parseInt(req.reject);
            loadPieData(accepted, rejected)
        }
        var x = (new Date()).getTime();
        var y = parseFloat(_hash_5m);
        var y1 = parseFloat(_av);
        series.addPoint([x, y], true, true);
        series1.addPoint([x, y1], true, true)
    }

    function updateHome() {
        var oUpdate;
        oUpdate = new AJAX("get_home.cgi?num=" + Math.random(),
            function (t) {
                try {
                    eval(t)
                } catch (e) {
                    console.log("updateHome err:" + e.message)
                }
            });
        oUpdate.doGet();
        setTimeout(updateHome, 15000)
    }
    updateHome();
    var linechart;
    var lineOptions = {
        chart: {
            renderTo: "linechart",
            type: "spline",
            events: {
                load: function () {
                    series = this.series[0];
                    series1 = this.series[1]
                }
            },
            spacing: [10, 300, 15, 10],
        },
        title: {
            text: "Computing monitoring"
        },
        xAxis: {
            type: "datetime",
            tickPixelInterval: 50
        },
        credits: {
            enabled: false
        },
        yAxis: [{
            tickPositions: [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            labels: {
                formatter: function () {
                    return this.value + "THs"
                },
                style: {
                    color: "#7cb5ec"
                }
            },
            title: {
                text: "THS",
                style: {
                    color: "#7cb5ec"
                }
            },
        }],
        colors: ["#ee8a96", "#bfbf00", "#06C", "#036", "#000"],
        legend: {
            align: "right",
            layout: "vertical",
            x: 100,
            floating: true,
            itemMarginBottom: 30,
        },
        exporting: {
            enabled: false
        },
        series: [{
            name: "Real-time",
            data: (function () {
                var data = [],
                    time = (new Date()).getTime(),
                    i;
                for (i = -_i; i <= 0; i++) {
                    data.push({
                        x: time + i * 1000,
                        y: null
                    })
                }
                return data
            })(),
            marker: {
                enabled: false
            },
        },
        {
            name: "Average",
            data: (function () {
                var data = [],
                    time = (new Date()).getTime(),
                    i;
                for (i = -_i; i <= 0; i++) {
                    data.push({
                        x: time + i * 1000,
                        y: null
                    })
                }
                return data
            })(),
            marker: {
                enabled: false
            },
        }]
    };
    linechart = new Highcharts.Chart(lineOptions);
    var piechart;
    var options = {
        chart: {
            renderTo: "piechart",
            type: "pie",
            backgroundColor: "rgba(0,0,0,0)"
        },
        credits: {
            enabled: false
        },
        title: {
            text: ""
        },
        plotOptions: {
            pie: {
                slicedOffset: 0,
                dataLabels: {
                    format: "<b>{point.name}</b><br> {point.percentage:.1f} %",
                    distance: -45,
                    style: {
                        textOutline: "1px 1px #130c0e",
                        fontSize: "12px",
                        color: "#FFFFFF"
                    }
                },
                startAngle: -90,
                endAngle: -90,
            }
        },
        colors: ["#1d953f", "#ed1941"],
        series: [{
            name: "pool",
            data: [{
                name: "Accepted",
                y: 0
            },
            {
                name: "Rejected",
                y: 0
            }]
        }]
    };
    function loadPieData(accepted, rejected) {
        options.series[0].data = [{
            name: "Accepted",
            y: accepted
        },
        {
            name: "Rejected",
            y: rejected
        }];
        piechart = new Highcharts.Chart(options)
    };


// Offset: 0x2A0431

    function checkLogin(){ 
    	if (loginform.username.value == "") { 
    		alert("Username is null!");
    		return false;
    	}
    	if(loginform.passwd.value == ""){ 
			alert("Password is null!");
    		return false;
    	}
    	loginform.submit();
    }      
    

// Offset: 0x0ADF71
function AJAX(url, callback) { var req = AJAX_init(); req.onreadystatechange = AJAX_processRequest; function AJAX_init() { if (window.XMLHttpRequest) { return new XMLHttpRequest() }

// Offset: 0x0AE07E
function AJAX_processRequest() { if (req.readyState == 4) { if (req.status == 200) { if (callback) { callback(req.responseText) }

// Offset: 0x0AE3A3
function logout(o) { var win = confirm("Are you sure you want to logout?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "logout.cgi"; cgiform.submit() }

// Offset: 0x0AE46A
function reboot(o) { var win = confirm("Are you sure you want to reboot?"); var p = o.getAttribute("id"); if (win == true) { cgiform.action = "reboot_btn.cgi"; cgiform.submit() }

// Offset: 0x0AE535
function tabChange(o) { var url = o.getAttribute("pageid") + ".cgi"; cgiform.action = url; cgiform.submit() }

// Offset: 0x0AE5A3
function UpdatePageCallback(req) { var pageId = req.pageId; var emList = document.getElementsByTagName("strong"); for (var index = 0; index < emList.length; index++) { if (emList[index].getAttribute("pageid") == pageId) { emList[index].setAttribute("class", "cur"); emList[index].previousElementSibling.setAttribute("class", "iconfont") }

// Offset: 0x0AE76B
function formatDateTime(inputTime) { var date = new Date(inputTime); var y = date.getFullYear(); var m = date.getMonth() + 1; m = m < 10 ? ("0" + m) : m; var d = date.getDate(); d = d < 10 ? ("0" + d) : d; var h = date.getHours(); h = h < 10 ? ("0" + h) : h; var minute = date.getMinutes(); var second = date.getSeconds(); minute = minute < 10 ? ("0" + minute) : minute; second = second < 10 ? ("0" + second) : second; return y + "-" + m + "-" + d + " " + h + ":" + minute + ":" + second }

// Offset: 0x0AE955
function minerinfoCallback(req) { req.hwtype !== undefined ? req.hwtype : ""; req.mac !== undefined ? req.mac : ""; req.ipv4 !== undefined ? req.ipv4 : ""; req.version !== undefined ? req.version : ""; $("hwtype").innerHTML = req.hwtype; $("loadtime").innerHTML = formatDateTime(new Date().getTime()); var html = ""; var css = ""; if (req.sys_status == "1") { html = "Online"; css = "status on" }

// Offset: 0x0AEBC0
function updateMinerInfo() { var oUpdate; oUpdate = new AJAX("get_minerinfo.cgi?num=" + Math.random(), function (t) { try { eval(t) }

// Offset: 0x0AECA1
function updateLog() { var oUpdate; oUpdate = new AJAX("updatecglog.cgi?num=" + Math.random(), function (t) { try { eval(t) }

