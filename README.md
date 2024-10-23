# Corkwin

Corkwin is the porting of Corkscrew to Windows. Like Corkscrew this tool allows you to tunnel SSH through HTTP proxies but from Windows.

## Installation

Download the executable in the latest Gitub release assets.

## Usage

To connect through an HTTP proxy with OpenSSH, you can use the following command (replace `server.example.com`, `proxy.example.com` and `8080` with correct values):

```
ssh server.example.com -o "ProxyCommand=corkwin proxy.example.com 8080 %h %p"
```

> [!NOTE]
> Make sure that corkwin is avaible from the current working directory when runnig this command.
