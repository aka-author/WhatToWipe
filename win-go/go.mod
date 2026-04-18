module whatrwipe/win-go

go 1.22

require (
	github.com/lxn/walk v0.0.0-20210112085537-c389da54e794
	github.com/lxn/win v0.0.0-20210218163916-a377121e959e
	golang.org/x/image v0.18.0
	golang.org/x/sys v0.15.0
)

require gopkg.in/Knetic/govaluate.v3 v3.0.0 // indirect

replace github.com/lxn/walk => ../../samples/third_party/walk
