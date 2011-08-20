<%
local print = print
local nginx = nginx
local req = nginx.request

local res = nginx.http({
  method = "GET",
  version = "1.1",
  url = "www.nginx.org/index.html",
  headers = {},
  body = ""
})
if res.status == nginx.ERROR then
  print("error")
  return
end
%>
<html>
<head>
</head>
<body>
<hr>
<%=res.status or ""%>
<hr>
<table border="1">
<% for k,v in pairs(res.headers) do %>
<tr><td><%=k%></td><td><%=v%></td></tr>
<% end %>
</table>
<hr>
<%=#res.body%>
<hr>
<%=res.body or ""%>
<hr>
request_time: <%=req.request_time%>ms
</body>
</html>