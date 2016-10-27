var particle = new Particle();
var token;

$( "#particle-form" ).submit(function() {
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
      //need to select device (maybe a modal?)
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

$( "#key-form" ).submit(function() {
  //process form and login with account
  var accessKey = $("#access-key").val();
  var remember = $("#particle-remember").val(); //TODO: Implement
  //Verify accessKey then set token equal to it
});

$('.login-form a').click(function(){
   $('form').animate({height: "toggle", opacity: "toggle"}, "slow");
});
