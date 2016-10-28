var particle = new Particle();
var token;
var deviceID;

$(document).ready(function() {
  $(document).on('click', '#device', function() {
    if($(this).hasClass("device-clicked")) {
      $(this).toggleClass("device-clicked");
    } else {
      $("div").removeClass( "device-clicked" )
      $(this).addClass("device-clicked");
    }
  });
  $("#device-selected").click(function() {
    $('.alert-device-selection').hide(50);
    $('.alert-device-status').hide(50);
    if ($(".device-clicked")[0]){
      console.log("Status", $(".device-clicked #status").text());
      if($(".device-clicked #status").text() == "Online") {
        deviceID = $(".device-clicked #device-ID").text();
        //Continue to dashboard
      } else {
        $('.alert-device-status').show(100);
      }
    } else {
      $('.alert-device-selection').show(100);
    }
  });
});


$(".login-form a").click(function(){
  //Toggle form
  $('form').animate({height: "toggle", opacity: "toggle"}, "slow");
});

$("#particle-form").submit(function(event) {
  //Prevent refresh
  event.preventDefault();
  //process form and login with account
  var email = $("#email").val();
  var password = $("#password").val();
  var remember = $("#particle-remember").val(); //TODO: Implement
  var details = {};
  details["username"] = email;
  details["password"] = password;

  particle.login(details).then(
    function(data){
      //console.log('API call completed on promise resolve: ', data.body.access_token);
      $('.alert-particle-login').hide(50);
      token = data.body.access_token;
      selectDevice();
    },
    function(err) {
      //console.log('API call completed on promise fail: ', err);
      $('.alert-particle-login').show(100);
      var animationEnd = 'webkitAnimationEnd mozAnimationEnd MSAnimationEnd oanimationend animationend';
      $('.login-form').addClass('animated shake').one(animationEnd, function() {
        $('.login-form').removeClass('animated shake');
      });
    }
  );
});

$("#key-form").submit(function(event) {
  //Prevent refresh
  event.preventDefault();
  //process form and login with account
  var accessKey = $("#access-key").val();
  var remember = $("#particle-remember").val(); //TODO: Implement
  //TODO: Verify accessKey
  token = accessKey;
  selectDevice();
});

function selectDevice() {
  var deviceList = particle.listDevices({ auth: token });
  deviceList.then(
    function(devices){
      //console.log('Devices: ', devices);
      //Delete any html lefttover
      $('#devices').empty();

      //Add html for devices in modal
      var i = 0;
      devices.body.forEach(function (device) {
        //console.log('Device: ', device);
        var content = "";
        var connected;

        if(devices["connected"]) {
          connected = 'Online';
        } else {
          connected = 'Offline';
        }
        if(i != 0) {
          content += '<hr>';
        }

        content += '<div id="device"><div class="row"><div class="col-md-8 text-truncate">' + device.name +
            '</div><div class="col-md-4" id="status">' + connected + '</div></div><div class="row">' +
            '<div class="col-md-12"><small class="text-muted">ID: </small><small class="text-muted" ' +
            'id="device-ID">' + device.id + '</small></div></div></div>';
        $("#devices").append(content);
        i++;
      });

      //Show modal
      $('#devices-modal').modal();
    },
    function(err) {
      console.log('List devices call failed: ', err); //TODO: Handle this error
    }
  );
}
