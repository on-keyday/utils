source_filename= "test1.min"

define i32 @main(){
entry:
%teq0 = call i32 (i32, i32, i32) @mul_add(i32 1, i32 2, i32 3)
ret i32 %teq0
}

define i32 @mul_add(i32 %x, i32 %y, i32 %z){
entry:
%teq1 = mul i32 %x, %y
%teq2 = add i32 %teq1, %z
ret i32 %teq2
}

define void @test(i32, i32){
entry:
ret void
}


