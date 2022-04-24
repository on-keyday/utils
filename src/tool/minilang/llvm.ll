define i32 @main() {
entry:
br label %or.0.begin
or.0.begin:
br i1 4, label %or.3.end, label %or.1.then
or.1.then:
br label %or.2.done
or.2.done:
br label %or.3.end
or.3.end:
%tmp4 = phi i1 [true, %or.0.begin], [4, %or.2.done]
ret i32 0
}
